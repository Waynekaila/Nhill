#include "apiclient.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QUuid>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace nhc {

QVariantMap ApiResult::toVariant() const {
    QVariantMap m;
    m["ok"] = ok;
    m["status"] = status;
    m["data"] = data.toVariant();
    m["network"] = network;
    m["uncertain"] = uncertain;
    m["idempotencyKey"] = idempotencyKey;
    if (!ok) {
        QVariantMap e;
        e["code"] = errorCode;
        e["message"] = message;
        e["details"] = details.toVariantMap();
        m["error"] = e;
    }
    return m;
}

ApiClient::ApiClient(QObject* parent) : QObject(parent) {
    m_nam.setTransferTimeout(m_timeoutMs);
    m_nam.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    m_probe.setInterval(20000);
    connect(&m_probe, &QTimer::timeout, this, &ApiClient::checkConnection);
}

void ApiClient::setBaseUrl(const QString& url) {
    QUrl u(url.trimmed());
    if (u == m_base) return;
    m_base = u;
    emit baseUrlChanged();
}

bool ApiClient::urlAllowed(const QUrl& u) const {
    if (u.scheme() == QLatin1String("https")) return true;
    // Development only: plain HTTP to the local machine (or the Android emulator host).
    const QString h = u.host();
    return u.scheme() == QLatin1String("http") &&
           (h == QLatin1String("127.0.0.1") || h == QLatin1String("localhost") || h == QLatin1String("::1") ||
            h == QLatin1String("10.0.2.2"));
}

void ApiClient::setTokens(const QString& access, const QString& refresh) {
    m_access = access;
    m_refresh = refresh;
    emit tokensChanged();
}

void ApiClient::clearTokens() {
    m_access.clear();
    m_refresh.clear();
    m_rights.clear();
    emit tokensChanged();
}

QString ApiClient::normaliseServerUrl(const QString& input) const {
    QString v = input.trimmed();
    if (v.isEmpty()) return QString();
    if (!v.contains(QLatin1String("://"))) v.prepend(QLatin1String("https://"));
    QUrl u(v, QUrl::StrictMode);
    if (!u.isValid() || u.host().isEmpty()) return QString();
    // Only the origin is kept: the API lives at /api/v1 of the server.
    QUrl origin;
    origin.setScheme(u.scheme().toLower());
    origin.setHost(u.host().toLower());
    if (u.port() > 0) origin.setPort(u.port());
    return origin.toString();
}

void ApiClient::probeServer(const QString& input, const QJSValue& cb) {
    ApiResult res;
    const QString base = normaliseServerUrl(input);
    if (base.isEmpty()) {
        res.errorCode = QStringLiteral("invalid_url");
        callJs(cb, res);
        return;
    }
    const QUrl url = QUrl(base).resolved(QUrl(QStringLiteral("/api/v1/meta")));
    if (!urlAllowed(url)) {
        res.errorCode = QStringLiteral("insecure_url");
        callJs(cb, res);
        return;
    }
    QNetworkRequest req(url);
    req.setTransferTimeout(10000);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "Nhill-Client/" + QCoreApplication::applicationVersion().toUtf8());
    QNetworkReply* reply = m_probeNam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, cb, base]() {
        reply->deleteLater();
        ApiResult r;
        const QVariant st = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (!st.isValid()) {
            r.network = true;
            const auto err = reply->error();
            r.errorCode = err == QNetworkReply::OperationCanceledError || err == QNetworkReply::TimeoutError
                              ? QStringLiteral("timeout")
                          : err == QNetworkReply::SslHandshakeFailedError ? QStringLiteral("tls_error")
                                                                          : QStringLiteral("network_error");
            r.message = reply->errorString();
            callJs(cb, r);
            return;
        }
        r.status = st.toInt();
        const QJsonObject meta = QJsonDocument::fromJson(reply->readAll()).object();
        // A Nhill server identifies itself in GET /api/v1/meta.
        if (r.status != 200 || meta.value(QStringLiteral("product")).toString() != QLatin1String("Nhill") ||
            meta.value(QStringLiteral("api")).toString() != QLatin1String("v1")) {
            r.errorCode = QStringLiteral("not_nhill_server");
            callJs(cb, r);
            return;
        }
        r.ok = true;
        QJsonObject d{{QStringLiteral("url"), base},
                      {QStringLiteral("version"), meta.value(QStringLiteral("version"))},
                      {QStringLiteral("api"), meta.value(QStringLiteral("api"))},
                      {QStringLiteral("min_client_version"), meta.value(QStringLiteral("min_client_version"))},
                      {QStringLiteral("mode"), meta.value(QStringLiteral("mode")).toString(QStringLiteral("ready"))}};
        r.data = d;
        callJs(cb, r);
    });
}

QString ApiClient::newIdempotencyKey() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

void ApiClient::setConnection(ConnectionState s) {
    if (s == m_conn) return;
    m_conn = s;
    if (s == Offline) m_probe.start();
    else m_probe.stop();
    emit connectionStateChanged();
}

void ApiClient::checkConnection() {
    Request r;
    r.path = QStringLiteral("/health");
    r.auth = false;
    request(r, [](const ApiResult&) {});
}

void ApiClient::request(Request r, Callback cb) {
    QUrl url = m_base.resolved(QUrl(r.path));
    if (!urlAllowed(url)) {
        ApiResult res;
        res.errorCode = QStringLiteral("insecure_url");
        res.message = QStringLiteral("HTTPS is required");
        cb(res);
        return;
    }
    if (r.method == "GET" && r.retriesLeft == 0 && !r.refreshed) r.retriesLeft = 2;
    dispatch(std::move(r), std::move(cb));
}

void ApiClient::dispatch(Request r, Callback cb) {
    QUrl url = m_base.resolved(QUrl(r.path));
    QNetworkRequest req(url);
    req.setTransferTimeout(r.timeoutMs > 0 ? r.timeoutMs : m_timeoutMs);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Accept-Language", m_lang.toUtf8());
    req.setRawHeader("User-Agent", "Nhill-Client/" + QCoreApplication::applicationVersion().toUtf8());
    if (r.auth && !m_access.isEmpty()) req.setRawHeader("Authorization", "Bearer " + m_access.toUtf8());
    if (!r.idempotencyKey.isEmpty()) req.setRawHeader("Idempotency-Key", r.idempotencyKey.toUtf8());
    QByteArray payload = r.rawBody;
    if (payload.isEmpty() && !r.body.isUndefined() && !r.body.isNull()) {
        payload = r.body.isObject() ? QJsonDocument(r.body.toObject()).toJson(QJsonDocument::Compact)
                                    : QJsonDocument(r.body.toArray()).toJson(QJsonDocument::Compact);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    } else if (!r.contentType.isEmpty()) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, r.contentType);
    }
    QNetworkReply* reply = m_nam.sendCustomRequest(req, r.method, payload);
    ++m_pending;
    emit pendingChanged();

    connect(reply, &QNetworkReply::finished, this, [this, reply, r, cb]() mutable {
        reply->deleteLater();
        --m_pending;
        emit pendingChanged();
        ApiResult res;
        res.idempotencyKey = r.idempotencyKey;
        const QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (!statusAttr.isValid()) {
            // No HTTP response: offline, DNS, TLS or timeout.
            res.network = true;
            const auto err = reply->error();
            res.errorCode = err == QNetworkReply::OperationCanceledError || err == QNetworkReply::TimeoutError
                                ? QStringLiteral("timeout")
                                : err == QNetworkReply::SslHandshakeFailedError ? QStringLiteral("tls_error")
                                                                               : QStringLiteral("network_error");
            res.message = reply->errorString();
            res.uncertain = r.method != "GET";
            if (res.errorCode != QLatin1String("tls_error")) setConnection(Offline);
            if (r.method == "GET" && r.retriesLeft > 0) {
                r.retriesLeft--;
                int delay = r.retriesLeft == 1 ? 1000 : 3000;
                QTimer::singleShot(delay, this, [this, r, cb]() mutable { dispatch(std::move(r), std::move(cb)); });
                return;
            }
            cb(res);
            return;
        }
        res.status = statusAttr.toInt();
        res.raw = reply->readAll();
        const QByteArray rights = reply->rawHeader("X-Nhill-Rights");
        if (!rights.isEmpty() && rights != m_rights) {
            const bool first = m_rights.isEmpty();
            m_rights = rights;
            if (!first) emit rightsChanged(QString::fromLatin1(rights));
        }
        res.contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        setConnection(res.status >= 500 && res.status != 503 ? Degraded : Online);
        if (res.contentType.contains(QLatin1String("json")) && !res.raw.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(res.raw);
            res.data = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
        }
        res.ok = res.status >= 200 && res.status < 300;
        if (!res.ok) {
            QJsonObject e = res.data.toObject().value(QStringLiteral("error")).toObject();
            res.errorCode = e.value(QStringLiteral("code")).toString(QStringLiteral("http_%1").arg(res.status));
            res.message = e.value(QStringLiteral("message")).toString();
            res.details = e.value(QStringLiteral("details")).toObject();
            if (res.status == 401 && r.auth && !r.refreshed && !m_refresh.isEmpty() &&
                res.errorCode == QLatin1String("invalid_token")) {
                // Access token expired: refresh once, then replay the same request
                // (same idempotency key for writes).
                tryRefresh([this, r, cb](bool ok) mutable {
                    if (!ok) {
                        ApiResult expired;
                        expired.status = 401;
                        expired.errorCode = QStringLiteral("session_expired");
                        emit sessionExpired();
                        cb(expired);
                        return;
                    }
                    r.refreshed = true;
                    dispatch(std::move(r), std::move(cb));
                });
                return;
            }
            if (res.status == 402) emit subscriptionBlocked(res.errorCode);
            // A feature refused by the server: the displayed rights are out of date.
            if (res.status == 403 && res.errorCode == QLatin1String("feature_not_included"))
                emit rightsChanged(QString::fromLatin1(m_rights));
        }
        cb(res);
    });
}

void ApiClient::tryRefresh(std::function<void(bool)> done) {
    m_refreshWaiters.push_back(std::move(done));
    if (m_refreshing) return;
    m_refreshing = true;
    Request r;
    r.method = "POST";
    r.path = QStringLiteral("/api/v1/auth/refresh");
    r.auth = false;
    r.body = QJsonObject{{QStringLiteral("refresh_token"), m_refresh}};
    dispatch(r, [this](const ApiResult& res) {
        m_refreshing = false;
        bool ok = res.ok;
        if (ok) {
            QJsonObject o = res.data.toObject();
            m_access = o.value(QStringLiteral("access_token")).toString();
            m_refresh = o.value(QStringLiteral("refresh_token")).toString();
            emit tokensRefreshed(m_refresh);
        } else if (!res.network) {
            m_access.clear();
            m_refresh.clear();
            emit tokensChanged();
        }
        auto waiters = std::move(m_refreshWaiters);
        m_refreshWaiters.clear();
        for (auto& w : waiters) w(ok);
    });
}

void ApiClient::callJs(const QJSValue& cb, const ApiResult& r) {
    if (!cb.isCallable()) return;
    QJSEngine* engine = qjsEngine(this);
    if (!engine) return;
    QJSValue fn = cb;
    fn.call({engine->toScriptValue(r.toVariant())});
}

void ApiClient::get(const QString& path, const QJSValue& cb) {
    Request r;
    r.path = path;
    request(r, [this, cb](const ApiResult& res) { callJs(cb, res); });
}

void ApiClient::send(const QString& method, const QString& path, const QVariant& body, const QJSValue& cb,
                     const QString& idempotencyKey) {
    Request r;
    r.method = method.toUtf8();
    r.path = path;
    // A JS object passed from QML arrives as QVariant<QJSValue>: unwrap it first,
    // otherwise QJsonValue::fromVariant() silently yields null.
    QVariant v = body;
    if (v.metaType() == QMetaType::fromType<QJSValue>()) v = v.value<QJSValue>().toVariant();
    if (v.isValid() && !v.isNull()) r.body = QJsonValue::fromVariant(v);
    if (r.body.isUndefined() || r.body.isNull()) r.body = QJsonObject();
    r.idempotencyKey = idempotencyKey;
    request(r, [this, cb](const ApiResult& res) { callJs(cb, res); });
}

void ApiClient::sendLong(const QString& method, const QString& path, const QVariant& body, const QJSValue& cb,
                         int timeoutMs) {
    Request r;
    r.method = method.toUtf8();
    r.path = path;
    QVariant v = body;
    if (v.metaType() == QMetaType::fromType<QJSValue>()) v = v.value<QJSValue>().toVariant();
    if (v.isValid() && !v.isNull()) r.body = QJsonValue::fromVariant(v);
    if (r.body.isUndefined() || r.body.isNull()) r.body = QJsonObject();
    r.timeoutMs = timeoutMs;
    request(r, [this, cb](const ApiResult& res) { callJs(cb, res); });
}

void ApiClient::upload(const QString& method, const QString& path, const QUrl& localFile, const QJSValue& cb) {
    QFile f(localFile.isLocalFile() ? localFile.toLocalFile() : localFile.toString());
    ApiResult err;
    if (!f.open(QIODevice::ReadOnly)) {
        err.errorCode = QStringLiteral("file_unreadable");
        callJs(cb, err);
        return;
    }
    if (f.size() > 5 * 1024 * 1024) {
        err.errorCode = QStringLiteral("file_too_large");
        callJs(cb, err);
        return;
    }
    Request r;
    r.method = method.toUtf8();
    r.path = path;
    r.rawBody = f.readAll();
    const QString suffix = QFileInfo(f).suffix().toLower();
    r.contentType = suffix == QLatin1String("png")                                      ? QStringLiteral("image/png")
                    : (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) ? QStringLiteral("image/jpeg")
                    : suffix == QLatin1String("pdf") ? QStringLiteral("application/pdf")
                    : suffix == QLatin1String("csv") ? QStringLiteral("text/csv")
                                                     : QStringLiteral("application/octet-stream");
    request(r, [this, cb](const ApiResult& res) { callJs(cb, res); });
}

namespace {
QString mimeOf(const QString& file) {
    const QString suffix = QFileInfo(file).suffix().toLower();
    if (suffix == QLatin1String("pdf")) return QStringLiteral("application/pdf");
    if (suffix == QLatin1String("csv")) return QStringLiteral("text/csv");
    if (suffix == QLatin1String("zip")) return QStringLiteral("application/zip");
    return QStringLiteral("application/octet-stream");
}

// Hands a downloaded file to the system: viewer, or share sheet when `share`.
bool handOver(const QString& file, bool share, const QString& title) {
#ifdef Q_OS_ANDROID
    // file:// cannot leave the app on Android 7+: content:// through the FileProvider
    // (client/android/src/cd/nhill/business/NhillFiles.java).
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    return QJniObject::callStaticMethod<jboolean>(
        "cd/nhill/business/NhillFiles", "open",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;ZLjava/lang/String;)Z", context.object(),
        QJniObject::fromString(file).object<jstring>(), QJniObject::fromString(mimeOf(file)).object<jstring>(),
        jboolean(share), QJniObject::fromString(title).object<jstring>());
#else
    Q_UNUSED(share);
    Q_UNUSED(title);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(file));
#endif
}
}  // namespace

bool ApiClient::canShare() const {
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

void ApiClient::downloadAndShare(const QString& path, const QString& fileName, const QString& title, const QJSValue& cb) {
    fetchToFile(path, fileName, true, title, cb);
}

void ApiClient::download(const QString& path, const QString& fileName, const QJSValue& cb) {
    fetchToFile(path, fileName, false, fileName, cb);
}

void ApiClient::fetchToFile(const QString& path, const QString& fileName, bool share, const QString& title, const QJSValue& cb) {
    Request r;
    r.path = path;
    r.timeoutMs = 120000;   // reports and data exports are generated on request
    request(r, [this, cb, fileName, share, title](const ApiResult& res) {
        if (!res.ok) {
            callJs(cb, res);
            return;
        }
        QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        dir += QStringLiteral("/Nhill");
        QDir().mkpath(dir);
        QString safe = fileName;
        safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
        QString target = dir + QLatin1Char('/') + safe;
        QFile out(target);
        ApiResult done = res;
        if (!out.open(QIODevice::WriteOnly) || out.write(res.raw) != res.raw.size()) {
            done.ok = false;
            done.errorCode = QStringLiteral("file_write_failed");
        } else {
            out.close();
            done.data = target;
            // The file is saved even when no app can show it (done.data gives where).
            if (!handOver(target, share, title)) done.errorCode = QStringLiteral("no_viewer");
        }
        callJs(cb, done);
    });
}

void ApiClient::fetchImage(const QString& path, const QJSValue& cb) {
    Request r;
    r.path = path;
    request(r, [this, cb](const ApiResult& res) {
        ApiResult done = res;
        if (res.ok) {
            const QString mime = res.contentType.section(QLatin1Char(';'), 0, 0).trimmed();
            if (mime != QLatin1String("image/png") && mime != QLatin1String("image/jpeg")) {
                done.ok = false;
                done.errorCode = QStringLiteral("unsupported_image");
            } else {
                done.data = QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(res.raw.toBase64()));
            }
        }
        callJs(cb, done);
    });
}

}  // namespace nhc
