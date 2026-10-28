// HTTPS client for the Nhill API.
//  - Certificates are always verified (no ignoreSslErrors anywhere).
//  - Plain HTTP is accepted only for loopback development servers.
//  - Reads are retried automatically on network failure; writes never are:
//    the UI retries them explicitly with the SAME idempotency key, so a
//    request whose outcome is uncertain can never be applied twice.
#pragma once

#include <QJSValue>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <functional>

namespace nhc {

struct ApiResult {
    bool ok = false;
    int status = 0;
    QJsonValue data;
    QByteArray raw;
    QString contentType;
    QString errorCode;      // API error code, or network_error / timeout / tls_error / insecure_url
    QString message;
    QJsonObject details;
    bool network = false;   // no HTTP response at all
    bool uncertain = false; // write sent but outcome unknown (retry with the same key)
    QString idempotencyKey;
    QVariantMap toVariant() const;
};

class ApiClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(int pending READ pending NOTIFY pendingChanged)
    Q_PROPERTY(bool hasSession READ hasSession NOTIFY tokensChanged)

public:
    enum ConnectionState { Unknown = 0, Online = 1, Offline = 2, Degraded = 3 };
    Q_ENUM(ConnectionState)

    explicit ApiClient(QObject* parent = nullptr);

    QString baseUrl() const { return m_base.toString(); }
    void setBaseUrl(const QString& url);
    int connectionState() const { return m_conn; }
    int pending() const { return m_pending; }
    bool hasSession() const { return !m_access.isEmpty(); }
    void setTimeoutMs(int ms) { m_timeoutMs = ms; }
    void setLanguage(const QString& lang) { m_lang = lang; }

    // Tokens (the refresh token is persisted by Session, not here).
    void setTokens(const QString& access, const QString& refresh);
    // Administration console: short-lived bearer token kept in memory only
    // (never persisted, no refresh token).
    Q_INVOKABLE void setAdminToken(const QString& token) { setTokens(token, QString()); }
    Q_INVOKABLE void signOut() { clearTokens(); }
    void clearTokens();
    QString refreshToken() const { return m_refresh; }

    using Callback = std::function<void(const ApiResult&)>;
    struct Request {
        QByteArray method = "GET";
        QString path;
        QJsonValue body;
        QByteArray rawBody;
        QString contentType;
        QString idempotencyKey;
        bool auth = true;
        int retriesLeft = 0;
        bool refreshed = false;
        int timeoutMs = 0;          // 0 = default
    };
    void request(Request r, Callback cb);

    // QML API: callbacks receive a plain JS object (see ApiResult::toVariant).
    Q_INVOKABLE void get(const QString& path, const QJSValue& cb);
    Q_INVOKABLE void send(const QString& method, const QString& path, const QVariant& body, const QJSValue& cb,
                          const QString& idempotencyKey = QString());
    // Long-running operation (server installation): same as send() with a longer timeout.
    Q_INVOKABLE void sendLong(const QString& method, const QString& path, const QVariant& body, const QJSValue& cb,
                              int timeoutMs);
    Q_INVOKABLE void upload(const QString& method, const QString& path, const QUrl& localFile, const QJSValue& cb);
    // Downloads a file (PDF/CSV) into the user's Downloads/Nhill folder and opens it.
    Q_INVOKABLE void download(const QString& path, const QString& fileName, const QJSValue& cb);
    // Authenticated image fetch for display (e.g. the establishment logo): data = "data:<mime>;base64,...".
    Q_INVOKABLE void fetchImage(const QString& path, const QJSValue& cb);
    Q_INVOKABLE QString newIdempotencyKey() const;
    Q_INVOKABLE void checkConnection();
    // Checks that `url` answers as a Nhill server before it is saved (no token is ever
    // sent): cb({ok, url (normalised), version, api, min_client_version, error: {code}}).
    // Codes: insecure_url, invalid_url, network_error, timeout, tls_error, not_nhill_server.
    Q_INVOKABLE void probeServer(const QString& url, const QJSValue& cb);
    // Normalised form of an address typed by a user ("api.example.com" -> "https://api.example.com").
    Q_INVOKABLE QString normaliseServerUrl(const QString& url) const;

signals:
    void baseUrlChanged();
    void connectionStateChanged();
    void pendingChanged();
    void tokensChanged();
    void tokensRefreshed(const QString& refreshToken);
    void sessionExpired();
    void subscriptionBlocked(const QString& code);
    // Version of the user's rights sent by the server (X-Nhill-Rights) changed, or a
    // feature was refused: the session reloads /me so that the interface follows the plan.
    void rightsChanged(const QString& version);

private:
    void dispatch(Request r, Callback cb);
    void setConnection(ConnectionState s);
    void tryRefresh(std::function<void(bool)> done);
    bool urlAllowed(const QUrl& u) const;
    void callJs(const QJSValue& cb, const ApiResult& r);

    QNetworkAccessManager m_nam;
    QUrl m_base;
    QString m_access, m_refresh, m_lang = QStringLiteral("fr");
    ConnectionState m_conn = Unknown;
    int m_pending = 0;
    int m_timeoutMs = 15000;
    bool m_refreshing = false;
    std::vector<std::function<void(bool)>> m_refreshWaiters;
    QTimer m_probe;
    QByteArray m_rights;
    QNetworkAccessManager m_probeNam;   // server checks: separate, never carries tokens
};

}  // namespace nhc
