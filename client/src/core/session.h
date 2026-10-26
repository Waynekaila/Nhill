// Current user session: identity, role/permissions, establishment and
// subscription. Hiding a button is a convenience only — every permission is
// enforced by the server.
#pragma once

#include <QJSValue>
#include <QObject>
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

    Q_INVOKABLE bool can(const QString& permission) const;
    // Accepts the "session" object returned by login / creation / join endpoints.
    Q_INVOKABLE void start(const QVariantMap& session, const QJSValue& done = QJSValue());
    Q_INVOKABLE void reload(const QJSValue& done = QJSValue());
    // Tries to resume with the refresh token kept in the OS keychain.
    Q_INVOKABLE void restore(const QJSValue& done);
    Q_INVOKABLE void logout();

signals:
    void changed();
    void expired();

private:
    void setMe(const QVariantMap& me);
    void persist(const QString& refresh);
    void callJs(const QJSValue& cb, bool ok);
    ApiClient* m_api;
    SecureStore* m_store;
    bool m_loggedIn = false;
    QVariantMap m_me;
};

// OS credential store (Secret Service / Windows Credential Manager / Android
// Keystore via QtKeychain). When unavailable, nothing is persisted: the
// refresh token is never written in clear text.
class SecureStore : public QObject {
    Q_OBJECT
public:
    explicit SecureStore(QObject* parent = nullptr) : QObject(parent) {}
    void write(const QString& key, const QString& value, std::function<void(bool)> done);
    void read(const QString& key, std::function<void(bool, QString)> done);
    void remove(const QString& key);
    bool available() const { return m_available; }

private:
    bool m_available = true;
};

}  // namespace nhc
