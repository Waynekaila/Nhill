#include "session.h"

#include <QJSEngine>
#include <QJsonObject>
#include <QSettings>

#ifdef NHILL_HAVE_KEYCHAIN
#include <qt6keychain/keychain.h>
#endif

namespace nhc {

namespace {
// Only a boolean preference lives in QSettings; the token itself is in the secure store.
const QString kRememberKey = QStringLiteral("session/remember");
// One stored session per server (host and port): a token is only ever sent back to the
// server that issued it.
QString storeKey(const ApiClient* api) {
    const QUrl u(api->baseUrl());
    return QStringLiteral("refresh@") + u.host() + (u.port() > 0 ? QStringLiteral(":%1").arg(u.port()) : QString());
}
}  // namespace

Session::Session(ApiClient* api, SecureStore* store, QObject* parent) : QObject(parent), m_api(api), m_store(store) {
    m_remember = SecureStore::supported() && QSettings().value(kRememberKey, false).toBool();
    // Refresh tokens rotate: the stored one is replaced each time (when remembered).
    connect(api, &ApiClient::tokensRefreshed, this, [this](const QString& refresh) { persist(refresh); });
    connect(api, &ApiClient::sessionExpired, this, [this]() {
        forget();
        m_loggedIn = false;
        m_me.clear();
        emit changed();
        emit expired();
    });
    // Rights changed on the server (plan, role, access): reload them now.
    connect(api, &ApiClient::rightsChanged, this, [this](const QString& version) {
        if (!m_loggedIn || m_reloading) return;
        if (!version.isEmpty() && version == m_me.value(QStringLiteral("rights_version")).toString()) return;
        reload();
    });
}

bool Session::persistent() const { return SecureStore::supported() && m_store->available(); }
bool Session::rememberSupported() const { return SecureStore::supported(); }

void Session::setRemember(bool on) {
    if (on && !rememberSupported()) on = false;
    if (on == m_remember) return;
    m_remember = on;
    QSettings().setValue(kRememberKey, on);
    if (!on) forget();                                        // nothing stays on this device
    else if (m_loggedIn) persist(m_api->refreshToken());
    emit rememberChanged();
}

void Session::forget() { m_store->remove(storeKey(m_api)); }

bool Session::can(const QString& permission) const {
    return m_me.value(QStringLiteral("permissions")).toStringList().contains(permission);
}

bool Session::has(const QString& feature) const { return can(QStringLiteral("feature.") + feature); }

QVariantMap Session::limit(const QString& key) const {
    return entitlements().value(QStringLiteral("limits")).toMap().value(key).toMap();
}

void Session::discard() {
    m_api->clearTokens();
    m_loggedIn = false;
    m_me.clear();
    emit changed();
}

void Session::callJs(const QJSValue& cb, bool ok) {
    if (!cb.isCallable()) return;
    QJSValue fn = cb;
    fn.call({QJSValue(ok)});
}

void Session::setMe(const QVariantMap& me) {
    m_me = me;
    m_loggedIn = !me.isEmpty();
    QSettings().setValue(QStringLiteral("session/lastEstablishment"), establishment().value(QStringLiteral("id")));
    emit changed();
}

void Session::persist(const QString& refresh) {
    if (refresh.isEmpty() || !m_remember) return;
    m_store->write(storeKey(m_api), refresh, [this](bool ok) {
        if (ok) return;
        emit changed();          // persistent() is now false
        emit rememberFailed();
    });
}

void Session::start(const QVariantMap& session, const QJSValue& done) {
    const QString access = session.value(QStringLiteral("access_token")).toString();
    const QString refresh = session.value(QStringLiteral("refresh_token")).toString();
    m_api->setTokens(access, refresh);
    if (m_remember) persist(refresh);
    else forget();               // a token left by an earlier "remember me" is erased
    reload(done);
}

void Session::reload(const QJSValue& done) {
    ApiClient::Request r;
    r.path = QStringLiteral("/api/v1/me");
    m_reloading = true;
    m_api->request(r, [this, done](const ApiResult& res) {
        m_reloading = false;
        if (res.ok) setMe(res.data.toObject().toVariantMap());
        callJs(done, res.ok);
    });
}

void Session::restore(const QJSValue& done) {
    // 2.0.0 kept every session when a keyring was available (Linux): without a saved
    // preference, a token found in the store means "remember me" was the behaviour.
    const bool legacy = SecureStore::supported() && !QSettings().contains(kRememberKey);
    if (!m_remember && !legacy) {
        forget();
        callJs(done, false);
        return;
    }
    m_store->read(storeKey(m_api), [this, done, legacy](bool ok, QString refresh) {
        if (!ok || refresh.isEmpty()) {
            callJs(done, false);
            return;
        }
        if (legacy && !m_remember) {
            m_remember = true;
            QSettings().setValue(kRememberKey, true);
            emit rememberChanged();
        }
        ApiClient::Request r;
        r.method = "POST";
        r.path = QStringLiteral("/api/v1/auth/refresh");
        r.auth = false;
        r.body = QJsonObject{{QStringLiteral("refresh_token"), refresh}};
        m_api->request(r, [this, done](const ApiResult& res) {
            if (!res.ok) {
                if (!res.network) forget();   // revoked or expired: no retry with this token
                callJs(done, false);
                return;
            }
            start(res.data.toObject().toVariantMap(), done);
        });
    });
}

void Session::logout() {
    ApiClient::Request r;
    r.method = "POST";
    r.path = QStringLiteral("/api/v1/auth/logout");
    m_api->request(r, [](const ApiResult&) {});
    forget();
    m_api->clearTokens();
    m_loggedIn = false;
    m_me.clear();
    emit changed();
}

// --------------------------------------------------------------------- SecureStore
// No insecure fallback is ever enabled (QKeychain::Job::setInsecureFallback stays false):
// when the platform store refuses, the session is simply not remembered.

bool SecureStore::supported() {
#ifdef NHILL_HAVE_KEYCHAIN
    return true;
#else
    return false;
#endif
}

void SecureStore::write(const QString& key, const QString& value, std::function<void(bool)> done) {
#ifdef NHILL_HAVE_KEYCHAIN
    auto* job = new QKeychain::WritePasswordJob(m_service);
    job->setAutoDelete(true);
    job->setKey(key);
    job->setTextData(value);
    connect(job, &QKeychain::Job::finished, this, [this, job, done](QKeychain::Job*) {
        bool ok = job->error() == QKeychain::NoError;
        m_available = ok;
        m_lastError = ok ? QString() : job->errorString();
        if (!ok) qWarning("secure store: the session cannot be remembered on this device (%s)", qPrintable(m_lastError));
        if (done) done(ok);
    });
    job->start();
#else
    Q_UNUSED(key);
    Q_UNUSED(value);
    m_available = false;
    if (done) done(false);
#endif
}

void SecureStore::read(const QString& key, std::function<void(bool, QString)> done) {
#ifdef NHILL_HAVE_KEYCHAIN
    auto* job = new QKeychain::ReadPasswordJob(m_service);
    job->setAutoDelete(true);
    job->setKey(key);
    connect(job, &QKeychain::Job::finished, this, [this, job, done](QKeychain::Job*) {
        bool ok = job->error() == QKeychain::NoError;
        if (!ok && job->error() != QKeychain::EntryNotFound) m_lastError = job->errorString();
        done(ok, ok ? job->textData() : QString());
    });
    job->start();
#else
    Q_UNUSED(key);
    done(false, QString());
#endif
}

void SecureStore::remove(const QString& key, std::function<void(bool)> done) {
#ifdef NHILL_HAVE_KEYCHAIN
    auto* job = new QKeychain::DeletePasswordJob(m_service);
    job->setAutoDelete(true);
    job->setKey(key);
    connect(job, &QKeychain::Job::finished, this, [job, done](QKeychain::Job*) {
        // An entry that does not exist is already removed.
        if (done) done(job->error() == QKeychain::NoError || job->error() == QKeychain::EntryNotFound);
    });
    job->start();
#else
    Q_UNUSED(key);
    if (done) done(true);
#endif
}

}  // namespace nhc
