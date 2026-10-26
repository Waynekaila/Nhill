#include "session.h"

#include <QJSEngine>
#include <QJsonObject>
#include <QSettings>

#ifdef NHILL_HAVE_KEYCHAIN
#include <qt6keychain/keychain.h>
#endif

namespace nhc {

namespace {
const QString kService = QStringLiteral("Nhill");
QString storeKey(const ApiClient* api) { return QStringLiteral("refresh@") + QUrl(api->baseUrl()).host(); }
}  // namespace

Session::Session(ApiClient* api, SecureStore* store, QObject* parent) : QObject(parent), m_api(api), m_store(store) {
    connect(api, &ApiClient::tokensRefreshed, this, [this](const QString& refresh) { persist(refresh); });
    connect(api, &ApiClient::sessionExpired, this, [this]() {
        m_store->remove(storeKey(m_api));
        m_loggedIn = false;
        m_me.clear();
        emit changed();
        emit expired();
    });
}

bool Session::persistent() const { return m_store->available(); }

bool Session::can(const QString& permission) const {
    return m_me.value(QStringLiteral("permissions")).toStringList().contains(permission);
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
    if (refresh.isEmpty()) return;
    m_store->write(storeKey(m_api), refresh, [](bool) {});
}

void Session::start(const QVariantMap& session, const QJSValue& done) {
    const QString access = session.value(QStringLiteral("access_token")).toString();
    const QString refresh = session.value(QStringLiteral("refresh_token")).toString();
    m_api->setTokens(access, refresh);
    persist(refresh);
    reload(done);
}

void Session::reload(const QJSValue& done) {
    ApiClient::Request r;
    r.path = QStringLiteral("/api/v1/me");
    m_api->request(r, [this, done](const ApiResult& res) {
        if (res.ok) setMe(res.data.toObject().toVariantMap());
        callJs(done, res.ok);
    });
}

void Session::restore(const QJSValue& done) {
    m_store->read(storeKey(m_api), [this, done](bool ok, QString refresh) {
        if (!ok || refresh.isEmpty()) {
            callJs(done, false);
            return;
        }
        ApiClient::Request r;
        r.method = "POST";
        r.path = QStringLiteral("/api/v1/auth/refresh");
        r.auth = false;
        r.body = QJsonObject{{QStringLiteral("refresh_token"), refresh}};
        m_api->request(r, [this, done](const ApiResult& res) {
            if (!res.ok) {
                if (!res.network) m_store->remove(storeKey(m_api));
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
    m_store->remove(storeKey(m_api));
    m_api->clearTokens();
    m_loggedIn = false;
    m_me.clear();
    emit changed();
}

// --------------------------------------------------------------------- SecureStore

void SecureStore::write(const QString& key, const QString& value, std::function<void(bool)> done) {
#ifdef NHILL_HAVE_KEYCHAIN
    auto* job = new QKeychain::WritePasswordJob(kService);
    job->setAutoDelete(true);
    job->setKey(key);
    job->setTextData(value);
    connect(job, &QKeychain::Job::finished, this, [this, job, done](QKeychain::Job*) {
        bool ok = job->error() == QKeychain::NoError;
        if (!ok) m_available = false;
        done(ok);
    });
    job->start();
#else
    Q_UNUSED(key);
    Q_UNUSED(value);
    m_available = false;
    done(false);
#endif
}

void SecureStore::read(const QString& key, std::function<void(bool, QString)> done) {
#ifdef NHILL_HAVE_KEYCHAIN
    auto* job = new QKeychain::ReadPasswordJob(kService);
    job->setAutoDelete(true);
    job->setKey(key);
    connect(job, &QKeychain::Job::finished, this, [job, done](QKeychain::Job*) {
        done(job->error() == QKeychain::NoError, job->textData());
    });
    job->start();
#else
    Q_UNUSED(key);
    done(false, QString());
#endif
}

void SecureStore::remove(const QString& key) {
#ifdef NHILL_HAVE_KEYCHAIN
    auto* job = new QKeychain::DeletePasswordJob(kService);
    job->setAutoDelete(true);
    job->setKey(key);
    job->start();
#else
    Q_UNUSED(key);
#endif
}

}  // namespace nhc
