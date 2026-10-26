#pragma once

#include <QObject>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QTranslator>

#include "apiclient.h"
#include "formatter.h"
#include "session.h"

namespace nhc {

// User preferences (QSettings). No business data is ever stored locally.
class Preferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY changed)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)          // system | light | dark
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY changed)

public:
    explicit Preferences(QObject* parent = nullptr);
    QString serverUrl() const { return m_server; }
    void setServerUrl(const QString& u);
    QString theme() const { return m_theme; }
    void setTheme(const QString& t);
    QString language() const { return m_lang; }
    void setLanguage(const QString& l);
    bool systemDark() const;

signals:
    void changed();
    void languageChanged();

private:
    QString m_server, m_theme, m_lang;
};

// Translates API error codes into user messages (French / English via Qt Linguist).
class Errors : public QObject {
    Q_OBJECT
public:
    explicit Errors(QObject* parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE QString message(const QVariant& result) const;
    Q_INVOKABLE QString code(const QString& code, const QString& field = QString()) const;
    Q_INVOKABLE QString fieldName(const QString& field) const;
};

// Utilities for QML (window capture for the documentation tour, clipboard).
class Tools : public QObject {
    Q_OBJECT
public:
    explicit Tools(QObject* parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE bool grabWindow(QObject* window, const QString& path) const;
    Q_INVOKABLE void copyText(const QString& text) const;
    // IANA time zone of this device (e.g. "Africa/Kinshasa"), sent when creating an establishment.
    Q_INVOKABLE QString systemTimeZone() const;
};

// Owns the core objects and exposes them to QML as singletons in "Nhill.Core".
class AppContext : public QObject {
    Q_OBJECT
public:
    AppContext(QQmlApplicationEngine* engine, QObject* parent = nullptr);
    void registerTypes();
    ApiClient* api() { return &m_api; }
    Session* session() { return &m_session; }
    Preferences* prefs() { return &m_prefs; }
    void applyLanguage(const QString& lang);

private:
    void flushErrorReports();   // sends QML / critical errors to the server (Business, POS)
    QTimer m_errorTimer;
    QQmlApplicationEngine* m_engine;
    ApiClient m_api;
    SecureStore m_store;
    Session m_session;
    Formatter m_fmt;
    Validator m_check;
    Preferences m_prefs;
    Errors m_errors;
    Tools m_tools;
    QTranslator m_translator;
};

void registerFonts();

}  // namespace nhc
