// Locale-aware, exact formatting/parsing of amounts, quantities and dates.
// Amounts travel as decimal strings ("1234.50"); no floating point.
#pragma once

#include <QObject>
#include <QString>

namespace nhc {

class Formatter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

public:
    explicit Formatter(QObject* parent = nullptr) : QObject(parent) {}
    QString language() const { return m_lang; }
    void setLanguage(const QString& l);

    // "1234.5" + "CDF" -> "1 234,50 CDF" (fr) / "CDF 1,234.50" (en)
    Q_INVOKABLE QString money(const QString& plain, const QString& currency) const;
    // Same without currency code.
    Q_INVOKABLE QString amount(const QString& plain, const QString& currency) const;
    // User input -> plain decimal string, or "" when invalid (too many decimals, letters...).
    Q_INVOKABLE QString parseMoney(const QString& text, const QString& currency) const;
    Q_INVOKABLE QString quantity(const QString& plain) const;
    Q_INVOKABLE QString parseQuantity(const QString& text) const;
    Q_INVOKABLE QString percent(int basisPoints) const;
    Q_INVOKABLE QString date(const QString& iso) const;        // "2026-10-25" or ISO timestamp
    Q_INVOKABLE QString dateTime(const QString& iso) const;
    Q_INVOKABLE QString today() const;
    Q_INVOKABLE QString addDays(const QString& isoDate, int days) const;
    Q_INVOKABLE QString monthStart() const;
    Q_INVOKABLE QString yearStart() const;
    Q_INVOKABLE int minorUnits(const QString& currency) const;
    // Line preview computed with the shared C++ rules (same as the server).
    Q_INVOKABLE QVariantMap lineTotals(const QString& qty, const QString& unitPrice, int discountBp, int taxBp,
                                       const QString& currency) const;
    // Exact sum of decimal strings ("12.30" + "0.70" = "13.00").
    Q_INVOKABLE QString sum(const QStringList& plains, const QString& currency) const;
    // Percentages typed by the user <-> basis points (exact).
    Q_INVOKABLE int parsePercent(const QString& text) const;      // "12,5" -> 1250 ; -1 if invalid
    Q_INVOKABLE QString percentText(int basisPoints) const;       // 1250 -> "12,5" (no % sign)
    Q_INVOKABLE bool isNegative(const QString& plain) const { return plain.startsWith(QLatin1Char('-')); }
    Q_INVOKABLE bool isZero(const QString& plain) const;

signals:
    void languageChanged();

private:
    QString m_lang = QStringLiteral("fr");
};

class Validator : public QObject {
    Q_OBJECT
public:
    explicit Validator(QObject* parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE bool email(const QString& v) const;
    Q_INVOKABLE bool phone(const QString& v) const;
    Q_INVOKABLE QString password(const QString& v, const QString& email) const;  // "" if OK, issue code otherwise
    Q_INVOKABLE bool establishmentCode(const QString& v) const;
    Q_INVOKABLE QString formatCode(const QString& v) const;
};

}  // namespace nhc
