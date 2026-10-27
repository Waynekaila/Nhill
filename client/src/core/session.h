// Current user session: identity, role/permissions, establishment and
// subscription. Hiding a button is a convenience only — every permission is
// enforced by the server.
#pragma once

#include <QJSValue>
#include <QObject>
#include <functional>
#include <QStringList>
#include <QVariantMap>

#include "apiclient.h"

namespace nhc {

class SecureStore;

class Session : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(QVariantMap me READ me NOTIFY changed)
    Q_PROPERTY(QVariantMap establishment READ establishment NOTIFY changed)
    Q_PROPERTY(QVariantMap subscription READ subscription NOTIFY changed)
    Q_PROPERTY(QString role READ role NOTIFY changed)
    Q_PROPERTY(bool individual READ individual NOTIFY changed)
    Q_PROPERTY(bool writeAllowed READ writeAllowed NOTIFY changed)
    Q_PROPERTY(QString currency READ currency NOTIFY changed)
    Q_PROPERTY(bool persistent READ persistent NOTIFY changed)
    // "Remember me": the refresh token is kept in the OS credential store only when true.
    Q_PROPERTY(bool remember READ remember WRITE setRemember NOTIFY rememberChanged)
    // False when this build has no secure store at all (the option is then unavailable).
    Q_PROPERTY(bool rememberSupported READ rememberSupported CONSTANT)

public:
    Session(ApiClient* api, SecureStore* store, QObject* parent = nullptr);

    bool loggedIn() const { return m_loggedIn; }
    QVariantMap me() const { return m_me; }
    QVariantMap establishment() const { return m_me.value(QStringLiteral("establishment")).toMap(); }
    QVariantMap subscription() const { return m_me.value(QStringLiteral("subscription")).toMap(); }
    QString role() const { return m_me.value(QStringLiteral("role")).toString(); }
    bool individual() const { return establishment().value(QStringLiteral("kind")).toString() == QLatin1String("individual"); }
    bool writeAllowed() const { return subscription().value(QStringLiteral("write_allowed")).toBool(); }
    QString currency() const { return establishment().value(QStringLiteral("main_currency")).toString(); }
    bool persistent() const;
    bool remember() const { return m_remember; }
    void setRemember(bool on);
    bool rememberSupported() const;

    Q_INVOKABLE bool can(const QString& permission) const;
    // Accepts the "session" object returned by login / creation / join endpoints.
    Q_INVOKABLE void start(const QVariantMap& session, const QJSValue& done = QJSValue());
    Q_INVOKABLE void reload(const QJSValue& done = QJSValue());
    // Tries to resume with the refresh token kept in the OS keychain ("remember me").
    Q_INVOKABLE void restore(const QJSValue& done);
    Q_INVOKABLE void logout();

signals:
    void changed();
    void expired();
    void rememberChanged();
    // The secure store refused to keep the session (e.g. no keyring service on Linux).
    void rememberFailed();

private:
    void setMe(const QVariantMap& me);
    void persist(const QString& refresh);
    void callJs(const QJSValue& cb, bool ok);
    void forget();
    ApiClient* m_api;
    SecureStore* m_store;
    bool m_loggedIn = false;
    bool m_remember = false;
    QVariantMap m_me;
};

// OS credential store through QtKeychain: Windows Credential Manager (DPAPI-protected,
// per user), Android Keystore (key never leaves the Keystore; the token is stored
// encrypted with it), Secret Service / KWallet on Linux. When unavailable, nothing is
// persisted: the refresh token is never written in clear text.
class SecureStore : public QObject {
    Q_OBJECT
public:
    explicit SecureStore(QObject* parent = nullptr, const QString& service = QStringLiteral("Nhill"))
        : QObject(parent), m_service(service) {}
    void write(const QString& key, const QString& value, std::function<void(bool)> done);
    void read(const QString& key, std::function<void(bool, QString)> done);
    void remove(const QString& key, std::function<void(bool)> done = {});
    bool available() const { return m_available; }
    static bool supported();
    QString lastError() const { return m_lastError; }

private:
    QString m_service;
    bool m_available = true;
    QString m_lastError;
};

}  // namespace nhc
