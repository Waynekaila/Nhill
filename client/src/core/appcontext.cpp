#include "appcontext.h"

#include <nhill/version.hpp>

#include <QJsonObject>
#include <cstdio>
#include <QMutex>
#include <QSet>
#include <QSysInfo>

#include <QTimeZone>

#include <QClipboard>
#include <QCollator>
#include <QDateTime>
#include <QCoreApplication>
#include <QQuickWindow>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLocale>
#include <QPalette>
#include <QQmlEngine>
#include <QSettings>
#include <QStyleHints>

#include <algorithm>

namespace nhc {

namespace {
struct CountryRow {
    const char *code, *fr, *en, *currency;
};
// Generated from the Unicode CLDR (scripts/tools/gen-countries.py).
const CountryRow kCountries[] = {
#include "countries_data.inc"
};
}  // namespace

// --------------------------------------------------------------------- Preferences

Preferences::Preferences(QObject* parent) : QObject(parent) {
    QSettings s;
    m_server = s.value(QStringLiteral("prefs/serverUrl")).toString();
    if (qEnvironmentVariableIsSet("NHILL_SERVER")) m_server = qEnvironmentVariable("NHILL_SERVER");
    m_theme = s.value(QStringLiteral("prefs/theme"), QStringLiteral("system")).toString();
    QString sys = QLocale::system().name().startsWith(QLatin1String("en")) ? QStringLiteral("en") : QStringLiteral("fr");
    m_lang = s.value(QStringLiteral("prefs/language"), sys).toString();
    if (qEnvironmentVariableIsSet("NHILL_LANG")) m_lang = qEnvironmentVariable("NHILL_LANG");
}

void Preferences::setServerUrl(const QString& u) {
    QString v = u.trimmed();
    while (v.endsWith(QLatin1Char('/'))) v.chop(1);
    if (v == m_server) return;
    m_server = v;
    QSettings().setValue(QStringLiteral("prefs/serverUrl"), m_server);
    emit changed();
}

void Preferences::setTheme(const QString& t) {
    m_theme = t;
    QSettings().setValue(QStringLiteral("prefs/theme"), t);
    emit changed();
}

void Preferences::setLanguage(const QString& l) {
    if (l == m_lang) return;
    m_lang = l;
    QSettings().setValue(QStringLiteral("prefs/language"), l);
    emit languageChanged();
}

bool Preferences::systemDark() const {
    if (qEnvironmentVariableIsSet("NHILL_THEME")) return qEnvironmentVariable("NHILL_THEME") == QLatin1String("dark");
    const QPalette p = QGuiApplication::palette();
    return p.color(QPalette::Window).lightness() < 128;
}

// --------------------------------------------------------------------- Errors

QString Errors::fieldName(const QString& field) const {
    static const QHash<QString, const char*> names = {
        {"host", QT_TR_NOOP("Host")}, {"port", QT_TR_NOOP("Port")}, {"database", QT_TR_NOOP("Database")},
        {"user", QT_TR_NOOP("Administrator user")}, {"sslmode", QT_TR_NOOP("TLS")}, {"provider", QT_TR_NOOP("Provider")},
        {"first_admin", QT_TR_NOOP("First administrator")},
        {"email", QT_TR_NOOP("E-mail")}, {"password", QT_TR_NOOP("Password")}, {"trade_name", QT_TR_NOOP("Trade name")},
        {"owner.password", QT_TR_NOOP("Password")}, {"owner.email", QT_TR_NOOP("E-mail")}, {"phone", QT_TR_NOOP("Phone")},
        {"main_currency", QT_TR_NOOP("Currency")}, {"legal_ids.nif", QT_TR_NOOP("Tax number (NIF)")},
        {"legal_ids.rccm", QT_TR_NOOP("RCCM")}, {"legal_ids.id_nat", QT_TR_NOOP("National ID")}, {"sku", QT_TR_NOOP("Reference")},
        {"name", QT_TR_NOOP("Name")}, {"sale_price", QT_TR_NOOP("Sale price")}, {"purchase_price", QT_TR_NOOP("Purchase price")},
        {"amount", QT_TR_NOOP("Amount")}, {"quantity", QT_TR_NOOP("Quantity")}, {"lines", QT_TR_NOOP("Lines")},
        {"fx_rate", QT_TR_NOOP("Exchange rate")}, {"party_id", QT_TR_NOOP("Customer")}, {"description", QT_TR_NOOP("Description")},
        {"reason", QT_TR_NOOP("Reason")}, {"currency", QT_TR_NOOP("Currency")}, {"logo", QT_TR_NOOP("Logo")},
    };
    QString base = field;
    if (base.startsWith(QLatin1String("lines["))) base = QStringLiteral("lines");
    auto it = names.find(base);
    return it == names.end() ? field : tr(it.value());
}

QString Errors::code(const QString& c, const QString& field) const {
    static const QHash<QString, const char*> msgs = {
        {"network_error", QT_TR_NOOP("No connection to the server. Check your internet access.")},
        {"timeout", QT_TR_NOOP("The server did not answer in time.")},
        {"tls_error", QT_TR_NOOP("Secure connection refused: the server certificate is not valid.")},
        {"insecure_url", QT_TR_NOOP("The server address must use HTTPS.")},
        {"invalid_credentials", QT_TR_NOOP("Incorrect e-mail or password.")},
        {"invalid_reset_code", QT_TR_NOOP("Incorrect or expired code. Use the code of the last e-mail received, or ask for a new one.")},
        {"invalid_change_token", QT_TR_NOOP("This step has expired. Sign in again with the temporary password.")},
        {"temporary_password_expired", QT_TR_NOOP("This temporary password has expired. Ask your administrator for a new one.")},
        {"password_change_required", QT_TR_NOOP("Choose your own password first: sign in with the temporary password given by your administrator.")},
        {"account_shared", QT_TR_NOOP("This account is also used in another business: the person resets it with \"Forgot your password?\" on the sign-in screen.")},
        {"member_disabled", QT_TR_NOOP("Re-enable this person's access first.")},
        {"too_many_attempts", QT_TR_NOOP("Too many attempts. Please wait a few minutes.")},
        {"code_unusable", QT_TR_NOOP("This code is invalid, expired or no longer usable. Ask the owner of the business for the current code.")},
        {"join_ticket_expired", QT_TR_NOOP("The code check has expired. Enter the code again.")},
        {"activation_failed", QT_TR_NOOP("Activation refused: check the e-mail address and the activation code given by your manager.")},
        {"establishment_exists", QT_TR_NOOP("A business with this identifier is already registered. You can request access to it.")},
        {"email_taken", QT_TR_NOOP("An account already exists with this e-mail. Use its password to continue.")},
        {"subscription_suspended", QT_TR_NOOP("Your subscription is inactive: data can be read and exported, but not modified.")},
        {"establishment_suspended", QT_TR_NOOP("This business has been suspended by Nhill support.")},
        {"permission_denied", QT_TR_NOOP("Your role does not allow this action.")},
        {"session_expired", QT_TR_NOOP("Your session has expired. Please sign in again.")},
        {"insufficient_stock", QT_TR_NOOP("Not enough stock for this sale.")},
        {"document_immutable", QT_TR_NOOP("An issued document can no longer be modified. Use a credit note.")},
        {"already_issued", QT_TR_NOOP("This document has already been issued.")},
        {"user_limit_reached", QT_TR_NOOP("Your plan does not allow more users.")},
        {"already_member", QT_TR_NOOP("This person is already a member.")},
        {"sku_taken", QT_TR_NOOP("This reference is already used by another product.")},
        {"barcode_taken", QT_TR_NOOP("This barcode is already used.")},
        {"idempotency_key_reused", QT_TR_NOOP("This operation conflicts with a previous attempt. Reopen the form.")},
        {"invalid_transition", QT_TR_NOOP("This change of status is not allowed.")},
        {"invoice_not_creditable", QT_TR_NOOP("This invoice cannot be credited.")},
        {"document_not_payable", QT_TR_NOOP("This document cannot receive a payment.")},
        {"feature_not_in_plan", QT_TR_NOOP("This feature is not included in your plan.")},
        {"feature_not_included", QT_TR_NOOP("This feature is not part of the plan of your business. The screen has been updated.")},
        {"plan_limit_reached", QT_TR_NOOP("The limit of your plan is reached.")},
        {"plan_register_limit", QT_TR_NOOP("Your plan does not allow more tills.")},
        {"owner_only", QT_TR_NOOP("Only the owner of the business can do this.")},
        {"invalid_url", QT_TR_NOOP("This server address is not valid.")},
        {"not_nhill_server", QT_TR_NOOP("This address does not answer as a Nhill server. Check the address given by your provider.")},
        {"auto_renew_not_allowed", QT_TR_NOOP("Automatic renewal is not available for this plan.")},
        {"plan_not_self_service", QT_TR_NOOP("This plan is assigned by Nhill: contact us to change it.")},
        {"plan_inactive", QT_TR_NOOP("This plan is not active.")},
        {"open_ended_requires_free_plan", QT_TR_NOOP("Only a free or custom plan can be given without an end date.")},
        {"open_ended_grant_requires_free_plan", QT_TR_NOOP("Only a free or custom plan can be given without an end date.")},
        {"plan_audience_mismatch", QT_TR_NOOP("This plan is not available for your type of business.")},
        {"plan_user_limit", QT_TR_NOOP("You have more active users than this plan allows.")},
        {"category_in_use", QT_TR_NOOP("This category still contains products.")},
        {"not_found", QT_TR_NOOP("Not found.")},
        {"server_busy", QT_TR_NOOP("The server is busy. Try again in a moment.")},
        {"busy", QT_TR_NOOP("The server is busy. Try again in a moment.")},
        {"internal_error", QT_TR_NOOP("Unexpected server error. The incident has been logged.")},
        {"file_too_large", QT_TR_NOOP("The file is too large.")},
        {"file_unreadable", QT_TR_NOOP("The file cannot be read.")},
        {"file_write_failed", QT_TR_NOOP("The file could not be saved on this device.")},
        {"import_failed", QT_TR_NOOP("Import refused: some lines are invalid. Nothing was imported.")},
        {"invalid_setup_token", QT_TR_NOOP("Invalid or expired setup token. Ask for a new one.")},
        {"provider_unavailable", QT_TR_NOOP("The payment provider cannot be reached right now. Try again in a few minutes.")},
        {"provider_refused", QT_TR_NOOP("The payment provider refused the request.")},
        {"provider_not_configured", QT_TR_NOOP("This payment method is not fully configured by Nhill.")},
        {"provider_not_implemented", QT_TR_NOOP("This payment method is not available yet.")},
        {"provider_disabled", QT_TR_NOOP("This payment method is disabled.")},
        {"provider_currency_unsupported", QT_TR_NOOP("This payment method does not accept this currency.")},
        {"provider_incomplete", QT_TR_NOOP("Required settings are missing for this provider.")},
        {"promo_invalid", QT_TR_NOOP("Unknown or inactive promo code.")},
        {"promo_expired", QT_TR_NOOP("This promo code has expired.")},
        {"promo_not_applicable", QT_TR_NOOP("This promo code does not apply to this plan or duration.")},
        {"promo_exhausted", QT_TR_NOOP("This promo code has reached its limit of uses.")},
        {"promo_already_used", QT_TR_NOOP("This promo code has already been used by your establishment.")},
        {"promo_new_customers_only", QT_TR_NOOP("This promo code is reserved for new customers.")},
        {"promo_currency_mismatch", QT_TR_NOOP("This promo code is in another currency.")},
        {"pending_plan_change", QT_TR_NOOP("A plan change is already scheduled at the end of the period.")},
        {"order_not_payable", QT_TR_NOOP("This payment request can no longer be paid.")},
        {"order_not_cancelable", QT_TR_NOOP("This payment request can no longer be cancelled.")},
        {"ticket_closed", QT_TR_NOOP("This ticket is closed: open a new one.")},
        {"no_change", QT_TR_NOOP("Nothing to change.")},
        {"setup_token_expired", QT_TR_NOOP("The setup token has expired: restart the Nhill server to get a new one.")},
        {"database_unreachable", QT_TR_NOOP("Connection to the database refused.")},
        {"setup_failed", QT_TR_NOOP("The installation failed.")},
        {"already_installed", QT_TR_NOOP("This server is already installed.")},
        {"invalid_totp", QT_TR_NOOP("Incorrect authenticator code. Check the time of your phone.")},
        {"purchase_not_draft", QT_TR_NOOP("This purchase has already been received or cancelled.")},
        {"purchase_immutable", QT_TR_NOOP("A received purchase can no longer be modified.")},
        {"purchase_not_payable", QT_TR_NOOP("Receive the goods before recording a payment.")},
        {"owner_required", QT_TR_NOOP("Only the owner can do this.")},
        {"owner_immutable", QT_TR_NOOP("The owner's access cannot be changed.")},
        {"cannot_modify_self", QT_TR_NOOP("You cannot change your own role or access.")},
        {"no_change", QT_TR_NOOP("Nothing to change.")},
        {"period_required", QT_TR_NOOP("Choose a period.")},
        {"invalid_period", QT_TR_NOOP("The start date must be before the end date.")},
    };
    static const QHash<QString, const char*> reasons = {
        {"required", QT_TR_NOOP("%1 is required.")},
        {"invalid_email", QT_TR_NOOP("%1: invalid e-mail address.")},
        {"invalid_phone", QT_TR_NOOP("%1: invalid phone number.")},
        {"invalid_amount", QT_TR_NOOP("%1: invalid amount (check the number of decimals).")},
        {"exceeds_amount_due", QT_TR_NOOP("The amount exceeds what is still due.")},
        {"password_too_short", QT_TR_NOOP("The password must contain at least 8 characters.")},
        {"password_too_common", QT_TR_NOOP("This password is too common.")},
        {"password_same_as_email", QT_TR_NOOP("The password must differ from the e-mail address.")},
        {"password_repetitive", QT_TR_NOOP("The password is too repetitive.")},
        {"password_reused", QT_TR_NOOP("Choose a password different from the temporary one.")},
        {"fx_rate_required", QT_TR_NOOP("An exchange rate is required for a document in a foreign currency.")},
        {"credit_exceeds_invoice", QT_TR_NOOP("The credit note exceeds the amount of the invoice.")},
        {"unsupported_format", QT_TR_NOOP("Only PNG and JPEG images are accepted.")},
        {"corrupted_image", QT_TR_NOOP("The image is damaged.")},
        {"bad_dimensions", QT_TR_NOOP("The image must be between 16 and 4096 pixels wide and high.")},
        {"too_large", QT_TR_NOOP("The file is too large (2 MB maximum).")},
        {"not_an_active_customer", QT_TR_NOOP("Choose an active customer.")},
        {"invalid_identifier", QT_TR_NOOP("%1: invalid identifier.")},
        {"must_be_positive", QT_TR_NOOP("%1 must be greater than zero.")},
        {"too_short", QT_TR_NOOP("%1 is too short.")},
        // Sent by 0.1.x servers only (kept so a 2.0 console still explains it).
        {"admin_password_min_14", QT_TR_NOOP("Administrator passwords need at least 14 characters.")},
        {"invalid_quantity", QT_TR_NOOP("%1: invalid quantity.")},
        {"same_warehouse", QT_TR_NOOP("Choose two different warehouses.")},
        {"same_currency", QT_TR_NOOP("Choose two different currencies.")},
        {"invalid_rate", QT_TR_NOOP("Invalid exchange rate.")},
        {"account_currency_mismatch", QT_TR_NOOP("The account must be in the currency of the operation.")},
        {"immutable_create_new_code", QT_TR_NOOP("A rate cannot be changed: create a new tax code.")},
        {"exceeds_amount", QT_TR_NOOP("%1 exceeds the amount.")},
        {"at_least_one_line", QT_TR_NOOP("Add at least one line.")},
        {"unknown_product", QT_TR_NOOP("Unknown product.")},
        {"not_an_active_supplier", QT_TR_NOOP("Choose an active supplier.")},
        {"unsupported_currency", QT_TR_NOOP("This currency is not supported.")},
        {"corrupted_pdf", QT_TR_NOOP("The PDF file is damaged.")},
        {"out_of_range", QT_TR_NOOP("%1: value out of the allowed range.")},
        {"cash_count_required", QT_TR_NOOP("Enter the cash counted in the drawer (0 if the drawer is empty).")},
        {"required_for_invoice", QT_TR_NOOP("An invoice needs an identified customer: choose the customer first.")},
        {"a4_or_ticket", QT_TR_NOOP("Choose A4 or ticket.")},
        {"58_or_80", QT_TR_NOOP("Choose a 58 mm or 80 mm roll.")},
    };
    if (c == QLatin1String("validation_failed")) return QString();
    auto it = msgs.find(c);
    if (it != msgs.end()) return tr(it.value());
    auto r = reasons.find(c);
    if (r != reasons.end()) {
        QString m = tr(r.value());
        return m.contains(QLatin1String("%1")) ? m.arg(fieldName(field)) : m;
    }
    return tr("The operation failed (%1).").arg(c);
}

QString Errors::message(const QVariant& result) const {
    QVariantMap m = result.toMap();
    QVariantMap e = m.value(QStringLiteral("error")).toMap();
    QString c = e.value(QStringLiteral("code")).toString();
    if (m.value(QStringLiteral("uncertain")).toBool())
        return tr("The connection was lost before the server answered. The operation may have been recorded: "
                  "use \"Retry\", it will never be applied twice.");
    if (c == QLatin1String("plan_limit_reached")) {
        // Which limit, in the user's words, and how many are used.
        QVariantMap d = e.value(QStringLiteral("details")).toMap();
        const QString k = d.value(QStringLiteral("limit_key")).toString();
        const QString max = d.value(QStringLiteral("limit")).toString();
        if (k == QLatin1String("catalog.products.max")) return tr("Your plan allows %1 active products and services.").arg(max);
        if (k == QLatin1String("documents.month.max")) return tr("Your plan allows %1 documents per month. Credit notes stay possible.").arg(max);
        if (k == QLatin1String("stock.warehouses.max")) return tr("Your plan allows %1 warehouses.").arg(max);
        if (k == QLatin1String("storage.mb.max")) return tr("Your plan allows %1 MB of stored files.").arg(max);
        if (k == QLatin1String("pos.registers.max")) return tr("Your plan does not allow more tills.");
        return code(c);
    }
    if (c == QLatin1String("validation_failed")) {
        QVariantMap d = e.value(QStringLiteral("details")).toMap();
        QString reason = d.value(QStringLiteral("reason")).toString();
        QString field = d.value(QStringLiteral("field")).toString();
        QString msg = code(reason, field);
        if (msg.startsWith(tr("The operation failed (%1).").left(10))) return tr("%1: invalid value.").arg(fieldName(field));
        return msg;
    }
    return code(c);
}

// --------------------------------------------------------------------- Tools

bool Tools::grabWindow(QObject* window, const QString& path) const {
    auto* w = qobject_cast<QQuickWindow*>(window);
    if (!w) return false;
    return w->grabWindow().save(path);
}

void Tools::copyText(const QString& text) const {
    if (auto* cb = QGuiApplication::clipboard()) cb->setText(text);
}

// --------------------------------------------------------------------- AppContext

namespace {
// Technical errors captured from QML and qCritical, reported to the server for the
// Console error journal. Bounded and deduplicated; never contains form data.
struct PendingError {
    QString level, code, message, file;
};
QMutex gErrMutex;
QList<PendingError> gErrQueue;
QSet<QString> gErrSeen;
QtMessageHandler gPrevHandler = nullptr;

void errorCapture(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    if (gPrevHandler) gPrevHandler(type, ctx, msg);
    else std::fprintf(stderr, "%s\n", qPrintable(qFormatLogMessage(type, ctx, msg)));
    const QString file = QString::fromUtf8(ctx.file ? ctx.file : "");
    const bool qml = file.endsWith(QLatin1String(".qml"));
    if (!(type == QtCriticalMsg || type == QtFatalMsg || (type == QtWarningMsg && qml))) return;
    if (msg.contains(QLatin1String("password"), Qt::CaseInsensitive)) return;
    QMutexLocker l(&gErrMutex);
    const QString key = file + QLatin1Char(':') + QString::number(ctx.line) + QLatin1Char('|') + msg.left(120);
    if (gErrSeen.contains(key) || gErrSeen.size() > 200) return;
    gErrSeen.insert(key);
    if (gErrQueue.size() < 20)
        gErrQueue.append({type == QtWarningMsg ? QStringLiteral("warning") : type == QtFatalMsg ? QStringLiteral("fatal") : QStringLiteral("error"),
                          qml ? QStringLiteral("qml:%1:%2").arg(file.section(QLatin1Char('/'), -1)).arg(ctx.line) : QStringLiteral("critical"),
                          msg.left(1000), file});
}
}  // namespace

void AppContext::flushErrorReports() {
    if (!m_api.hasSession()) return;
    QList<PendingError> batch;
    {
        QMutexLocker l(&gErrMutex);
        batch.swap(gErrQueue);
    }
    const QString app = QCoreApplication::applicationName();
    const QString source = app.contains(QLatin1String("POS")) ? QStringLiteral("pos") : QStringLiteral("business");
    for (const auto& e : batch) {
        ApiClient::Request r;
        r.method = "POST";
        r.path = QStringLiteral("/api/v1/client-errors");
        QJsonObject b{{QStringLiteral("source"), source}, {QStringLiteral("level"), e.level}, {QStringLiteral("code"), e.code},
                      {QStringLiteral("message"), e.message}, {QStringLiteral("app_version"), QCoreApplication::applicationVersion()},
                      {QStringLiteral("platform"), QSysInfo::productType()}};
        r.body = b;
        m_api.request(r, [](const ApiResult&) {});
    }
}

AppContext::AppContext(QQmlApplicationEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine), m_session(&m_api, &m_store) {
    // The console signs in with an administrator token: its errors stay local.
    if (!QCoreApplication::applicationName().contains(QLatin1String("Console")) && !gPrevHandler) {
        gPrevHandler = qInstallMessageHandler(errorCapture);
        m_errorTimer.setInterval(30000);
        connect(&m_errorTimer, &QTimer::timeout, this, &AppContext::flushErrorReports);
        m_errorTimer.start();
    }
    m_api.setBaseUrl(m_prefs.serverUrl());
    connect(&m_prefs, &Preferences::changed, this, [this] {
        // Another server: the current session (tokens of the previous server) ends here,
        // without any request, so that no token is sent to the new address.
        if (QUrl(m_prefs.serverUrl()) != QUrl(m_api.baseUrl())) m_session.discard();
        m_api.setBaseUrl(m_prefs.serverUrl());
    });
    connect(&m_prefs, &Preferences::languageChanged, this, [this] { applyLanguage(m_prefs.language()); });
}

void AppContext::applyLanguage(const QString& lang) {
    QCoreApplication::removeTranslator(&m_translator);
    if (m_translator.load(QStringLiteral(":/i18n/nhill_%1.qm").arg(lang))) QCoreApplication::installTranslator(&m_translator);
    m_fmt.setLanguage(lang);
    m_api.setLanguage(lang);
    if (m_engine) m_engine->retranslate();
}

void AppContext::registerTypes() {
    const char* uri = "Nhill.Core";
    qmlRegisterSingletonInstance(uri, 1, 0, "Api", &m_api);
    qmlRegisterSingletonInstance(uri, 1, 0, "Session", &m_session);
    qmlRegisterSingletonInstance(uri, 1, 0, "Fmt", &m_fmt);
    qmlRegisterSingletonInstance(uri, 1, 0, "Check", &m_check);
    qmlRegisterSingletonInstance(uri, 1, 0, "Prefs", &m_prefs);
    qmlRegisterSingletonInstance(uri, 1, 0, "Errors", &m_errors);
    qmlRegisterSingletonInstance(uri, 1, 0, "Tools", &m_tools);
    qmlRegisterUncreatableType<ApiClient>(uri, 1, 0, "ApiClient", QStringLiteral("singleton"));
    applyLanguage(m_prefs.language());
}

void registerFonts() {
    for (const char* f : {":/fonts/IBMPlexSans-Regular.ttf", ":/fonts/IBMPlexSans-Medium.ttf", ":/fonts/IBMPlexSans-SemiBold.ttf",
                          ":/fonts/IBMPlexSans-Bold.ttf", ":/fonts/IBMPlexMono-Regular.ttf", ":/fonts/IBMPlexMono-Medium.ttf"})
        QFontDatabase::addApplicationFont(QString::fromLatin1(f));
}

}  // namespace nhc

namespace nhc {
QString Tools::systemTimeZone() const { return QString::fromUtf8(QTimeZone::systemTimeZoneId()); }

QVariantList Tools::timeZones() const {
    struct Z {
        int offset;
        QString id;
    };
    std::vector<Z> zones;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const QByteArray& id : QTimeZone::availableTimeZoneIds()) {
        const QString s = QString::fromUtf8(id);
        // Geographic IANA names only (Region/City); legacy aliases and fixed offsets are left out.
        if (!s.contains(QLatin1Char('/')) || s.startsWith(QLatin1String("Etc/")) || s.startsWith(QLatin1String("SystemV/")))
            continue;
        zones.push_back({QTimeZone(id).offsetFromUtc(now), s});
    }
    std::sort(zones.begin(), zones.end(), [](const Z& a, const Z& b) { return a.offset != b.offset ? a.offset < b.offset : a.id < b.id; });
    QVariantList out;
    for (const auto& z : zones) {
        const int m = std::abs(z.offset) / 60;
        const QString off = QStringLiteral("UTC%1%2:%3").arg(z.offset < 0 ? QLatin1Char('-') : QLatin1Char('+'))
                                .arg(m / 60, 2, 10, QLatin1Char('0')).arg(m % 60, 2, 10, QLatin1Char('0'));
        QString label = z.id;
        label.replace(QLatin1Char('_'), QLatin1Char(' '));
        out.append(QVariantMap{{QStringLiteral("value"), z.id}, {QStringLiteral("text"), label + QStringLiteral(" (") + off + QLatin1Char(')')}});
    }
    out.append(QVariantMap{{QStringLiteral("value"), QStringLiteral("UTC")}, {QStringLiteral("text"), QStringLiteral("UTC")}});
    return out;
}

QVariantList Tools::countries(const QString& lang) const {
    const bool fr = lang == QLatin1String("fr");
    std::vector<const CountryRow*> rows;
    for (const auto& c : kCountries) rows.push_back(&c);
    QCollator col(QLocale(fr ? QLocale::French : QLocale::English));
    std::sort(rows.begin(), rows.end(), [&](const CountryRow* a, const CountryRow* b) {
        return col.compare(QString::fromUtf8(fr ? a->fr : a->en), QString::fromUtf8(fr ? b->fr : b->en)) < 0;
    });
    QVariantList out;
    for (const auto* c : rows)
        out.append(QVariantMap{{QStringLiteral("value"), QString::fromLatin1(c->code)},
                               {QStringLiteral("text"), QString::fromUtf8(fr ? c->fr : c->en)},
                               {QStringLiteral("currency"), QString::fromLatin1(c->currency)}});
    return out;
}

QString Tools::countryCurrency(const QString& code) const {
    for (const auto& c : kCountries)
        if (code == QLatin1String(c.code)) return QString::fromLatin1(c.currency);
    return QString();
}

QString Tools::systemCountry() const {
    const QString name = QLocale::system().name();  // e.g. "fr_FR"
    const QString cc = name.section(QLatin1Char('_'), 1, 1).left(2).toUpper();
    for (const auto& c : kCountries)
        if (cc == QLatin1String(c.code)) return cc;
    return QString();
}
QString Tools::publisherName() const { return QString::fromUtf8(nhill::kPublisherName); }
QString Tools::contactEmail() const { return QString::fromUtf8(nhill::kContactEmail); }
QString Tools::supportEmail() const { return QString::fromUtf8(nhill::kSupportEmail); }
}  // namespace nhc
