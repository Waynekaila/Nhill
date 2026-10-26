#include "services/report.hpp"

#include "nhill/money.hpp"
#include "pdf/pdf.hpp"
#include "services/sales.hpp"
#include "util/crypto.hpp"
#include "util/csv.hpp"

namespace nh {

Period period(const Ctx& ctx) {
    Period p;
    auto f = ctx.query("from"), t = ctx.query("to");
    if (!f || !in::isDate(*f) || !t || !in::isDate(*t)) throw badRequest("period_required", "from and to (YYYY-MM-DD) are required");
    if (*f > *t) throw badRequest("invalid_period");
    p.from = *f;
    p.to = *t;
    return p;
}

std::string normalizeSpaces(std::string s) {
    for (size_t p; (p = s.find("\xE2\x80\xAF")) != std::string::npos;) s.replace(p, 3, "\xC2\xA0");
    return s;
}

void sendReport(Env& env, Ctx& ctx, db::Tx& tx, const Report& r, const std::string& name, const Period* per) {
    std::string format = ctx.query("format").value_or("json");
    const std::string lang = ctx.actor.lang;
    if (format == "csv") {
        csv::Writer w;
        std::vector<std::string> head;
        for (const auto& k : r.keys) head.push_back(env.i18n.t(lang, k));
        w.row(head);
        for (const auto& row : r.rows) w.row(row);
        ctx.file(w.str(), "text/csv; charset=utf-8", name + ".csv");
        return;
    }
    if (format == "pdf") {
        pdf::TableReport t;
        t.title = env.i18n.t(lang, r.titleKey);
        if (per) t.subtitle = env.i18n.t(lang, "report.period") + " : " + per->from + " → " + per->to;
        if (!r.extraSubtitle.empty()) t.subtitle += (t.subtitle.empty() ? "" : " · ") + r.extraSubtitle;
        if (!r.orgName.empty()) {
            t.establishmentName = r.orgName;
        } else {
            auto e = tx.one("SELECT e.trade_name, o.mime, encode(o.data,'hex') FROM establishments e LEFT JOIN objects o "
                            "ON o.id = e.logo_object_id WHERE e.id = $1",
                            ctx.actor.establishmentId);
            t.establishmentName = db::str(e[0]);
            if (!e[1].is_null()) {
                t.logoMime = db::str(e[1]);
                t.logoBytes = *crypto::fromHex(db::str(e[2]));
            }
        }
        for (const auto& k : r.keys) t.columns.push_back(env.i18n.t(lang, k));
        for (size_t i = 0; i < r.types.size(); ++i) {
            t.rightAlign.push_back(r.types[i] == Col::Money || r.types[i] == Col::Qty || r.types[i] == Col::Int);
            t.widths.push_back(r.types[i] == Col::Text ? 2.2 : 1.0);
        }
        // Totals per currency for every money column (amounts are never mixed).
        int curCol = -1;
        for (size_t i = 0; i < r.types.size(); ++i)
            if (r.types[i] == Col::Money && r.currencyCol[i] >= 0) curCol = r.currencyCol[i];
        std::map<std::string, std::vector<int64_t>> sums;
        if (curCol >= 0) {
            for (const auto& row : r.rows) {
                const std::string& cur = row[static_cast<size_t>(curCol)];
                if (cur.empty()) continue;  // rows without amounts (e.g. an employee with no sale)
                auto& v = sums[cur];
                v.resize(row.size(), 0);
                for (size_t i = 0; i < row.size(); ++i)
                    if (r.types[i] == Col::Money)
                        if (auto m = nhill::parseMoney(row[i], minorUnits(cur))) v[i] += *m;
            }
        }
        std::vector<std::vector<std::string>> rows = r.rows;
        for (const auto& [cur, v] : sums) {
            std::vector<std::string> tot(r.keys.size());
            tot[0] = env.i18n.t(lang, "report.total");
            tot[static_cast<size_t>(curCol)] = cur;
            for (size_t i = 0; i < v.size(); ++i)
                if (r.types[i] == Col::Money) tot[i] = money(v[i], cur);
            rows.push_back(tot);
        }
        size_t dataRows = r.rows.size();
        for (size_t ri = 0; ri < rows.size(); ++ri) {
            const auto& row = rows[ri];
            std::vector<std::string> out;
            for (size_t i = 0; i < row.size(); ++i) {
                if (r.types[i] == Col::Date && ri < dataRows) {
                    out.push_back(formatDate(row[i], lang) + (row[i].size() > 10 ? row[i].substr(10) : ""));
                } else if (r.types[i] == Col::Money && r.currencyCol[i] >= 0) {
                    std::string cur = row[static_cast<size_t>(r.currencyCol[i])];
                    auto v = nhill::parseMoney(row[i], minorUnits(cur));
                    out.push_back(v ? normalizeSpaces(nhill::formatDecimal(*v, minorUnits(cur), lang)) : row[i]);
                } else if (r.types[i] == Col::Qty) {
                    auto v = nhill::parseQuantity(row[i]);
                    out.push_back(v ? normalizeSpaces(nhill::formatDecimal(*v, 3, lang, true)) : row[i]);
                } else {
                    out.push_back(row[i]);
                }
            }
            if (ri < dataRows) t.rows.push_back(out);
            else if (sums.size() == 1) t.totalRow = out;
            else t.rows.push_back(out);
        }
        t.footerLeft = env.i18n.t(lang, "doc.generated_by");
        t.pageLabel = env.i18n.t(lang, "doc.page");
        t.landscape = r.landscape || r.keys.size() > 7;
        ctx.file(pdf::renderTable(t, pdf::Fonts::fromDir(env.cfg.fontDir)), "application/pdf", name + ".pdf");
        return;
    }
    json cols = json::array();
    for (size_t i = 0; i < r.keys.size(); ++i) {
        static const char* names[] = {"text", "money", "qty", "date", "int"};
        cols.push_back({{"key", r.keys[i]}, {"label", env.i18n.t(lang, r.keys[i])}, {"type", names[static_cast<int>(r.types[i])]}});
    }
    ctx.json(200, {{"columns", cols}, {"rows", r.rows}});
}

}  // namespace nh
