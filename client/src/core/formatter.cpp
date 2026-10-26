#include "formatter.h"

#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QTimeZone>
#include <QVariantMap>

#include "nhill/establishment_code.hpp"
#include "nhill/money.hpp"
#include "nhill/validation.hpp"

namespace nhc {

namespace {
std::string s(const QString& q) { return q.toStdString(); }
QString q(const std::string& s) { return QString::fromStdString(s); }
int mu(const QString& currency) {
    int m = nhill::currencyMinorUnits(s(currency));
    return m < 0 ? 2 : m;
}
}  // namespace

void Formatter::setLanguage(const QString& l) {
    if (l == m_lang) return;
    m_lang = l;
    emit languageChanged();
}

int Formatter::minorUnits(const QString& currency) const { return mu(currency); }

QString Formatter::amount(const QString& plain, const QString& currency) const {
    auto v = nhill::parseMoney(s(plain), mu(currency));
    if (!v) return plain;
    return q(nhill::formatDecimal(*v, mu(currency), s(m_lang)));
}

QString Formatter::money(const QString& plain, const QString& currency) const {
    auto v = nhill::parseMoney(s(plain), mu(currency));
    if (!v) return plain;
    return q(nhill::formatMoney(*v, s(currency), s(m_lang)));
}

QString Formatter::parseMoney(const QString& text, const QString& currency) const {
    auto v = nhill::parseMoney(s(text), mu(currency));
    if (!v || *v < 0) return QString();
    return q(nhill::toPlainDecimal(*v, mu(currency)));
}

QString Formatter::quantity(const QString& plain) const {
    auto v = nhill::parseQuantity(s(plain));
    if (!v) return plain;
    return q(nhill::formatDecimal(*v, 3, s(m_lang), true));
}

QString Formatter::parseQuantity(const QString& text) const {
    auto v = nhill::parseQuantity(s(text));
    if (!v || *v <= 0) return QString();
    std::string out = nhill::toPlainDecimal(*v, 3);
    while (!out.empty() && out.back() == '0') out.pop_back();
    if (!out.empty() && out.back() == '.') out.pop_back();
    return q(out);
}

QString Formatter::percent(int bp) const {
    QString n = q(nhill::formatDecimal(bp, 2, s(m_lang), true));
    return m_lang == QLatin1String("fr") ? n + QStringLiteral(" %") : n + QStringLiteral("%");
}

QString Formatter::date(const QString& iso) const {
    if (iso.isEmpty()) return QString();
    QDate d = QDate::fromString(iso.left(10), Qt::ISODate);
    if (!d.isValid()) return iso;
    if (iso.size() > 10) {
        QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
        if (dt.isValid()) d = dt.toLocalTime().date();
    }
    QLocale loc(m_lang == QLatin1String("fr") ? QLocale::French : QLocale::English);
    return m_lang == QLatin1String("fr") ? d.toString(QStringLiteral("dd/MM/yyyy")) : loc.toString(d, QStringLiteral("d MMM yyyy"));
}

QString Formatter::dateTime(const QString& iso) const {
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid()) return iso;
    dt = dt.toLocalTime();
    return date(dt.date().toString(Qt::ISODate)) + QStringLiteral(" ") + dt.time().toString(QStringLiteral("HH:mm"));
}

QString Formatter::today() const { return QDate::currentDate().toString(Qt::ISODate); }

QString Formatter::addDays(const QString& isoDate, int days) const {
    QDate d = QDate::fromString(isoDate, Qt::ISODate);
    return d.isValid() ? d.addDays(days).toString(Qt::ISODate) : isoDate;
}

QString Formatter::monthStart() const {
    QDate t = QDate::currentDate();
    return QDate(t.year(), t.month(), 1).toString(Qt::ISODate);
}

QString Formatter::yearStart() const { return QDate(QDate::currentDate().year(), 1, 1).toString(Qt::ISODate); }

QString Formatter::sum(const QStringList& plains, const QString& currency) const {
    int u = mu(currency);
    int64_t total = 0;
    for (const auto& p : plains) {
        auto v = nhill::parseMoney(s(p), u);
        if (v) total += *v;
    }
    return q(nhill::toPlainDecimal(total, u));
}

int Formatter::parsePercent(const QString& text) const {
    if (text.trimmed().isEmpty()) return 0;
    auto v = nhill::parseDecimal(s(text), 2);
    if (!v || *v < 0 || *v > 10000) return -1;
    return static_cast<int>(*v);
}

QString Formatter::percentText(int bp) const { return q(nhill::formatDecimal(bp, 2, s(m_lang), true)); }

bool Formatter::isZero(const QString& plain) const {
    auto v = nhill::parseDecimal(s(plain), 6);
    return v && *v == 0;
}

QVariantMap Formatter::lineTotals(const QString& qty, const QString& unitPrice, int discountBp, int taxBp,
                                  const QString& currency) const {
    QVariantMap m;
    auto qv = nhill::parseQuantity(s(qty));
    auto pv = nhill::parseMoney(s(unitPrice), mu(currency));
    if (!qv || !pv || *qv <= 0 || *pv < 0) {
        m["valid"] = false;
        return m;
    }
    auto la = nhill::computeLine({*qv, *pv, discountBp, taxBp});
    if (!la) {
        m["valid"] = false;
        return m;
    }
    int u = mu(currency);
    m["valid"] = true;
    m["net"] = q(nhill::toPlainDecimal(la->netMinor, u));
    m["tax"] = q(nhill::toPlainDecimal(la->taxMinor, u));
    m["total"] = q(nhill::toPlainDecimal(la->totalMinor, u));
    return m;
}

// --------------------------------------------------------------------- Validator

bool Validator::email(const QString& v) const { return nhill::validation::isValidEmail(s(v)); }
bool Validator::phone(const QString& v) const { return nhill::validation::normalizePhone(s(v)).has_value(); }

QString Validator::password(const QString& v, const QString& email) const {
    auto issue = nhill::validation::checkPassword(s(v), s(email));
    return issue == nhill::validation::PasswordIssue::None ? QString()
                                                               : q(nhill::validation::passwordIssueCode(issue));
}

bool Validator::establishmentCode(const QString& v) const { return nhill::code::normalize(s(v)).has_value(); }

QString Validator::formatCode(const QString& v) const {
    auto c = nhill::code::normalize(s(v));
    return c ? q(nhill::code::display(*c)) : v;
}

}  // namespace nhc
