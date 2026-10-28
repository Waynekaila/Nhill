// Current user session: identity, role/permissions, establishment and
// subscription.
//
// Rights (3.0.0) come from the server only (/me): "permissions" holds the permissions of
// the role reduced to the plan of the establishment, plus one "feature.<key>" entry per
// feature of the plan. The applications show a menu, a tab, a button or a setting only
// when Session.can(...) / Session.has(...) is true, so a feature outside the plan is
// invisible; the server refuses it anyway. When the rights change (plan change, role
// change, expiry), the server sends a new X-Nhill-Rights version and the session reloads
// /me at once, so the interface follows without restarting.
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
    // Plan of the establishment: {access, plan, trial, features[], limits{key: {max, used}}}.
    Q_PROPERTY(QVariantMap entitlements READ entitlements NOTIFY changed)
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
    QVariantMap entitlements() const { return m_me.value(QStringLiteral("entitlements")).toMap(); }
    QString currency() const { return establishment().value(QStringLiteral("main_currency")).toString(); }
    bool persistent() const;
    bool remember() const { return m_remember; }
    void setRemember(bool on);
    bool rememberSupported() const;

    Q_INVOKABLE bool can(const QString& permission) const;
    // True when the plan of the establishment includes the feature (e.g. "pos", "purchases").
    Q_INVOKABLE bool has(const QString& feature) const;
    // Limit of the plan for a capability ("catalog.products.max"): {max, used}, max null = unlimited.
    Q_INVOKABLE QVariantMap limit(const QString& key) const;
    // Ends the session on this device only, without any request (used when the server
    // address changes: tokens are never sent to another server).
    Q_INVOKABLE void discard();
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
    bool m_reloading = false;
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
