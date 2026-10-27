// Sales documents: quotes, orders, invoices, credit notes.
//   draft (editable) -> issued (immutable, numbered, snapshotted) -> paid / credited
#include <algorithm>

#include "nhill/money.hpp"
#include "pdf/pdf.hpp"
#include "services/entitlements.hpp"
#include "services/sales.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"

namespace nh {

std::string invoiceStatusFor(int64_t total, int64_t paid, int64_t credited) {
    if (credited >= total && total > 0) return "credited";
    if (paid + credited >= total) return "paid";
    if (paid + credited > 0) return "partially_paid";
    return "issued";
}

namespace {

const std::set<std::string> kTypes = {"quote", "order", "invoice", "credit_note"};

}  // namespace

// "2026-10-25" -> "25/10/2026" (fr) or "25 Oct 2026" (en).
std::string formatDate(const std::string& iso, const std::string& lang) {
    if (iso.size() < 10) return iso;
    if (lang == "fr") return iso.substr(8, 2) + "/" + iso.substr(5, 2) + "/" + iso.substr(0, 4);
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int m = std::stoi(iso.substr(5, 2));
    return iso.substr(8, 2) + " " + (m >= 1 && m <= 12 ? months[m - 1] : "") + " " + iso.substr(0, 4);
}

namespace {

std::string percent(int bp, const std::string& lang) {
    // 1600 -> "16 %" (fr) / "16%" (en); 1250 -> "12,5 %"
    std::string s = nhill::formatDecimal(bp, 2, lang, true);
    return lang == "fr" ? s + "\xC2\xA0%" : s + "%";
}

}  // namespace

namespace sales {

// Parses and prices the lines of a draft; returns computed totals.
std::pair<std::vector<LineCalc>, nhill::DocumentTotals> priceLines(db::Tx& tx, const json& b, const std::string& currency,
                                                                       const std::string& rounding) {
    if (!b.contains("lines") || !b["lines"].is_array()) throw invalid("lines", "required");
    const json& arr = b["lines"];
    if (arr.empty()) throw invalid("lines", "at_least_one_line");
    if (arr.size() > 500) throw invalid("lines", "too_many_lines");
    auto defTax = tx.opt("SELECT rate_bp, code FROM tax_rates WHERE is_default AND active LIMIT 1");
    std::vector<LineCalc> lines;
    std::vector<nhill::LineInput> inputs;
    for (size_t i = 0; i < arr.size(); ++i) {
        const json& l = arr[i];
        const std::string f = "lines[" + std::to_string(i) + "].";
        if (!l.is_object()) throw invalid(f, "must_be_object");
        LineCalc lc;
        lc.productId = in::optUuid(l, "product_id");
        std::optional<pqxx::row> prod;
        if (lc.productId) {
            prod = tx.opt("SELECT p.name, p.unit, p.sale_price_minor, p.currency, t.rate_bp, t.code, p.active "
                          "FROM products p LEFT JOIN tax_rates t ON t.id = p.tax_rate_id WHERE p.id = $1",
                          *lc.productId);
            if (!prod) throw invalid(f + "product_id", "unknown_product");
        }
        lc.description = in::optStr(l, "description", 500).value_or(prod ? db::str((*prod)[0]) : "");
        if (lc.description.empty()) throw invalid(f + "description", "required");
        lc.unit = in::optStr(l, "unit", 16).value_or(prod ? db::str((*prod)[1]) : "unit");
        lc.qty = in::reqQty(l, "quantity");
        if (lc.qty <= 0) throw invalid(f + "quantity", "must_be_positive");
        if (auto p = in::optMoney(l, "unit_price", currency)) {
            lc.unitPrice = *p;
        } else if (prod && db::str((*prod)[3]) == currency) {
            lc.unitPrice = (*prod)[2].as<int64_t>();
        } else {
            throw invalid(f + "unit_price", "required");
        }
        lc.discountBp = in::optInt(l, "discount_bp", 0, 0, 10000);
        if (auto t = in::optUuid(l, "tax_rate_id")) {
            auto tr = tx.opt("SELECT rate_bp, code FROM tax_rates WHERE id = $1 AND active", *t);
            if (!tr) throw invalid(f + "tax_rate_id", "unknown_tax_rate");
            lc.taxBp = (*tr)[0].as<int>();
            lc.taxCode = db::str((*tr)[1]);
        } else if (prod && !(*prod)[4].is_null()) {
            lc.taxBp = (*prod)[4].as<int>();
            lc.taxCode = db::str((*prod)[5]);
        } else if (defTax) {
            lc.taxBp = (*defTax)[0].as<int>();
            lc.taxCode = db::str((*defTax)[1]);
        }
        inputs.push_back({lc.qty, lc.unitPrice, lc.discountBp, lc.taxBp});
        lines.push_back(lc);
    }
    auto totals = nhill::computeDocument(
        inputs, rounding == "per_rate" ? nhill::TaxRounding::PerRate : nhill::TaxRounding::PerLine);
    if (!totals) throw invalid("lines", "amount_overflow");
    return {lines, *totals};
}

void writeLines(db::Tx& tx, const std::string& est, const std::string& docId, const std::vector<LineCalc>& lines,
                const nhill::DocumentTotals& t) {
    tx.q("DELETE FROM sales_document_lines WHERE document_id = $1", docId);
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& l = lines[i];
        const auto& a = t.lines[i];
        tx.q("INSERT INTO sales_document_lines (establishment_id, document_id, line_no, product_id, description, "
             "quantity_milli, unit, unit_price_minor, discount_bp, tax_rate_bp, tax_code, gross_minor, discount_minor, "
             "net_minor, tax_minor, total_minor) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16)",
             est, docId, static_cast<int>(i + 1), l.productId, l.description, l.qty, l.unit, l.unitPrice, l.discountBp,
             l.taxBp, l.taxCode, a.grossMinor, a.discountMinor, a.netMinor, a.taxMinor, a.totalMinor);
    }
}

json breakdownJson(const nhill::DocumentTotals& t) {
    json arr = json::array();
    for (const auto& b : t.taxes) arr.push_back({{"rate_bp", b.rateBp}, {"base_minor", b.baseMinor}, {"tax_minor", b.taxMinor}});
    return arr;
}

void history(db::Tx& tx, const Actor& actor, const std::string& docId, const std::optional<std::string>& from,
             const std::string& to, const std::string& note) {
    tx.q("INSERT INTO document_status_history (establishment_id, document_id, from_status, to_status, actor_id, note) "
         "VALUES ($1,$2,$3,$4,$5,$6)",
         actor.establishmentId, docId, from, to, actor.userId, note);
}

std::string formatNumber(const json& profile, const std::string& lang, const std::string& type, const std::string& year,
                         int64_t seq) {
    const json& num = profile.at("numbering");
    std::string pattern = num.value("pattern", "{prefix}-{year}-{seq6}");
    std::string prefix = num.at("prefixes").at(lang).at(type);
    auto replace = [&](const std::string& key, const std::string& val) {
        auto p = pattern.find(key);
        if (p != std::string::npos) pattern.replace(p, key.size(), val);
    };
    std::string s6 = std::to_string(seq);
    if (s6.size() < 6) s6.insert(0, 6 - s6.size(), '0');
    replace("{prefix}", prefix);
    replace("{year}", year);
    replace("{seq6}", s6);
    replace("{seq}", std::to_string(seq));
    return pattern;
}

}  // namespace sales

using namespace sales;

// --------------------------------------------------------------------- document view

json loadSalesDocument(db::Tx& tx, const std::string& id) {
    auto d = tx.one(
        "SELECT d.id, d.doc_type, d.status, d.number, d.party_id, p.name, d.currency, d.language, d.issue_date::text, "
        "d.due_date::text, d.valid_until::text, d.main_currency, d.fx_rate::text, d.fx_rate_date::text, d.fx_rate_source, "
        "d.tax_rounding, d.gross_minor, d.discount_minor, d.net_minor, d.tax_minor, d.total_minor, d.paid_minor, "
        "d.credited_minor, d.tax_breakdown, d.notes, d.terms, d.related_document_id, r.number, d.restock, "
        "d.fiscal_reference, d.pdf_sha256, nh_iso(d.issued_at), nh_iso(d.created_at), nh_iso(d.updated_at), "
        "d.profile_code, d.profile_version "
        "FROM sales_documents d JOIN parties p ON p.id = d.party_id "
        "LEFT JOIN sales_documents r ON r.id = d.related_document_id WHERE d.id = $1",
        id);
    std::string cur = db::str(d[6]);
    auto nullable = [](const pqxx::field& f) { return f.is_null() ? json(nullptr) : json(db::str(f)); };
    auto m = [&](int i) { return money(d[i].as<int64_t>(), cur); };
    json taxes = json::array();
    for (const auto& t : json::parse(db::str(d[23])))
        taxes.push_back({{"rate_bp", t["rate_bp"]}, {"base", money(t["base_minor"].get<int64_t>(), cur)},
                         {"tax", money(t["tax_minor"].get<int64_t>(), cur)}});
    json lines = json::array();
    for (const auto& l : tx.q(
             "SELECT l.line_no, l.product_id, pr.sku, l.description, l.quantity_milli, l.unit, l.unit_price_minor, "
             "l.discount_bp, l.tax_rate_bp, l.tax_code, l.net_minor, l.tax_minor, l.total_minor FROM sales_document_lines l "
             "LEFT JOIN products pr ON pr.id = l.product_id WHERE l.document_id = $1 ORDER BY l.line_no",
             id))
        lines.push_back({{"line_no", l[0].as<int>()},
                         {"product_id", nullable(l[1])},
                         {"sku", nullable(l[2])},
                         {"description", db::str(l[3])},
                         {"quantity", qty(l[4].as<int64_t>())},
                         {"unit", db::str(l[5])},
                         {"unit_price", money(l[6].as<int64_t>(), cur)},
                         {"discount_bp", l[7].as<int>()},
                         {"tax_rate_bp", l[8].as<int>()},
                         {"tax_code", db::str(l[9])},
                         {"net", money(l[10].as<int64_t>(), cur)},
                         {"tax", money(l[11].as<int64_t>(), cur)},
                         {"total", money(l[12].as<int64_t>(), cur)}});
    json hist = json::array();
    for (const auto& h : tx.q("SELECT h.from_status, h.to_status, COALESCE(u.full_name,''), h.note, nh_iso(h.created_at) "
                              "FROM document_status_history h LEFT JOIN users u ON u.id = h.actor_id "
                              "WHERE h.document_id = $1 ORDER BY h.id",
                              id))
        hist.push_back({{"from", nullable(h[0])}, {"to", db::str(h[1])}, {"by", db::str(h[2])}, {"note", db::str(h[3])},
                        {"at", db::str(h[4])}});
    json pays = json::array();
    for (const auto& p : tx.q("SELECT id, direction, method, amount_minor, nh_iso(paid_at), reference FROM payments "
                              "WHERE document_id = $1 ORDER BY paid_at",
                              id))
        pays.push_back({{"id", db::str(p[0])}, {"direction", db::str(p[1])}, {"method", db::str(p[2])},
                        {"amount", money(p[3].as<int64_t>(), cur)}, {"paid_at", db::str(p[4])},
                        {"reference", db::str(p[5])}});
    int64_t balance = d[20].as<int64_t>() - d[21].as<int64_t>() - d[22].as<int64_t>();
    return {{"id", db::str(d[0])},
            {"doc_type", db::str(d[1])},
            {"status", db::str(d[2])},
            {"number", nullable(d[3])},
            {"party_id", db::str(d[4])},
            {"party_name", db::str(d[5])},
            {"currency", cur},
            {"language", db::str(d[7])},
            {"issue_date", db::str(d[8])},
            {"due_date", nullable(d[9])},
            {"valid_until", nullable(d[10])},
            {"main_currency", db::str(d[11])},
            {"fx_rate", nullable(d[12])},
            {"fx_rate_date", nullable(d[13])},
            {"fx_rate_source", nullable(d[14])},
            {"tax_rounding", db::str(d[15])},
            {"gross", m(16)},
            {"discount", m(17)},
            {"net", m(18)},
            {"tax", m(19)},
            {"total", m(20)},
            {"paid", m(21)},
            {"credited", m(22)},
            {"balance", money(balance, cur)},
            {"taxes", taxes},
            {"notes", db::str(d[24])},
            {"terms", db::str(d[25])},
            {"related_document_id", nullable(d[26])},
            {"related_number", nullable(d[27])},
            {"restock", db::boolean(d[28])},
            {"fiscal_reference", d[29].is_null() ? json(nullptr) : json::parse(db::str(d[29]))},
            {"pdf_sha256", nullable(d[30])},
            {"issued_at", nullable(d[31])},
            {"created_at", db::str(d[32])},
            {"updated_at", db::str(d[33])},
            {"profile", nullable(d[34])},
            {"lines", lines},
            {"history", hist},
            {"payments", pays}};
}

// --------------------------------------------------------------------- PDF

std::string renderSalesPdf(Env& env, db::Tx& tx, const std::string& id) {
    json d = loadSalesDocument(tx, id);
    const std::string lang = d["language"];
    const std::string cur = d["currency"];
    auto row = tx.one(
        "SELECT d.seller_snapshot, d.buyer_snapshot, e.trade_name, e.legal_name, e.address, e.city, e.country_code, "
        "e.phone, e.email, e.vat_registered, e.compliance_profile_code, s.payment_instructions, s.invoice_footer, "
        "p.name, p.address, p.city, p.country_code, p.email, p.phone, p.tax_ids, o.mime, encode(o.data,'hex') "
        "FROM sales_documents d JOIN establishments e ON e.id = d.establishment_id "
        "JOIN establishment_settings s ON s.establishment_id = d.establishment_id JOIN parties p ON p.id = d.party_id "
        "LEFT JOIN objects o ON o.id = e.logo_object_id WHERE d.id = $1",
        id);
    const bool draft = d["status"] == "draft";
    json seller, buyer;
    if (!draft && !row[0].is_null()) {
        seller = json::parse(db::str(row[0]));
        buyer = json::parse(db::str(row[1]));
    } else {
        seller = {{"trade_name", db::str(row[2])}, {"legal_name", db::str(row[3])}, {"address", db::str(row[4])},
                  {"city", db::str(row[5])},       {"country", db::str(row[6])},    {"phone", db::str(row[7])},
                  {"email", db::str(row[8])},      {"vat_registered", db::boolean(row[9])},
                  {"profile", db::str(row[10])},   {"payment_instructions", db::str(row[11])},
                  {"footer", db::str(row[12])}};
        json ids = json::object();
        for (const auto& r : tx.q("SELECT kind, value FROM establishment_identifiers WHERE establishment_id = "
                                  "(SELECT establishment_id FROM sales_documents WHERE id = $1)",
                                  id))
            ids[db::str(r[0])] = db::str(r[1]);
        seller["legal_ids"] = ids;
        buyer = {{"name", db::str(row[13])}, {"address", db::str(row[14])}, {"city", db::str(row[15])},
                 {"country", db::str(row[16])}, {"email", db::str(row[17])}, {"phone", db::str(row[18])},
                 {"tax_ids", json::parse(db::str(row[19]))}};
    }
    const json& profile = env.profiles.get(seller.value("profile", std::string("INTL")));
    auto t = [&](const std::string& k) { return env.i18n.t(lang, k); };
    auto fmtMoney = [&](const std::string& plain) {
        auto v = nhill::parseMoney(plain, minorUnits(cur));
        std::string s = nhill::formatDecimal(v.value_or(0), minorUnits(cur), lang);
        // The PDF fonts carry U+00A0 for sure; normalise the narrow space.
        for (size_t p; (p = s.find("\xE2\x80\xAF")) != std::string::npos;) s.replace(p, 3, "\xC2\xA0");
        return s;
    };
    auto fmtQty = [&](const std::string& plain) {
        std::string s = nhill::formatDecimal(nhill::parseQuantity(plain).value_or(0), 3, lang, true);
        for (size_t p; (p = s.find("\xE2\x80\xAF")) != std::string::npos;) s.replace(p, 3, "\xC2\xA0");
        return s;
    };

    pdf::SalesDocView v;
    v.title = t("doc." + d["doc_type"].get<std::string>());
    v.number = d["number"].is_null() ? "" : d["number"].get<std::string>();
    if (draft) v.watermark = t("doc.draft_watermark");
    v.sellerName = seller.value("trade_name", "");
    auto addIf = [](std::vector<std::string>& vec, const std::string& s) {
        if (!s.empty()) vec.push_back(s);
    };
    if (seller.value("legal_name", "") != v.sellerName) addIf(v.sellerLines, seller.value("legal_name", ""));
    addIf(v.sellerLines, seller.value("address", ""));
    addIf(v.sellerLines, seller.value("city", ""));
    std::string contact = seller.value("phone", "");
    if (!seller.value("email", "").empty()) contact += (contact.empty() ? "" : " · ") + seller.value("email", "");
    addIf(v.sellerLines, contact);
    std::string ids;
    for (const auto& def : profile.at("legal_ids")) {
        std::string k = def.at("kind");
        if (seller.contains("legal_ids") && seller["legal_ids"].contains(k))
            ids += (ids.empty() ? "" : " · ") + t("id." + k) + " " + seller["legal_ids"][k].get<std::string>();
    }
    addIf(v.sellerLines, ids);
    v.buyerLabel = t("doc.bill_to");
    v.buyerName = buyer.value("name", "");
    addIf(v.buyerLines, buyer.value("address", ""));
    addIf(v.buyerLines, buyer.value("city", ""));
    std::string bids;
    if (buyer.contains("tax_ids"))
        for (auto it = buyer["tax_ids"].begin(); it != buyer["tax_ids"].end(); ++it)
            bids += (bids.empty() ? "" : " · ") + t("id." + it.key()) + " " + it.value().get<std::string>();
    addIf(v.buyerLines, bids);
    std::string bcontact = buyer.value("phone", "");
    if (!buyer.value("email", "").empty()) bcontact += (bcontact.empty() ? "" : " · ") + buyer.value("email", "");
    addIf(v.buyerLines, bcontact);
    if (!row[20].is_null()) {
        v.logoBytes = *crypto::fromHex(db::str(row[21]));
        v.logoMime = db::str(row[20]);
    }

    v.meta.push_back({t("doc.issue_date"), formatDate(d["issue_date"], lang)});
    if (!d["due_date"].is_null() && d["doc_type"] == "invoice")
        v.meta.push_back({t("doc.due_date"), formatDate(d["due_date"], lang)});
    if (!d["valid_until"].is_null() && d["doc_type"] == "quote")
        v.meta.push_back({t("doc.valid_until"), formatDate(d["valid_until"], lang)});
    if (!d["related_number"].is_null()) v.meta.push_back({t("doc.related"), d["related_number"]});
    v.meta.push_back({t("doc.currency"), cur});
    if (!d["fx_rate"].is_null()) {
        auto rate = nhill::ExchangeRate::parse(d["fx_rate"].get<std::string>());
        v.meta.push_back({t("doc.fx"), "1 " + cur + " = " + (rate ? rate->toString() : "") + " " +
                                           d["main_currency"].get<std::string>() + " (" +
                                           formatDate(d["fx_rate_date"].get<std::string>(), lang) + ")"});
    }

    v.columns = {t("doc.col.ref"), t("doc.col.description"), t("doc.col.qty"), t("doc.col.unit_price"),
                 t("doc.col.discount"), t("doc.col.tax"), t("doc.col.total")};
    for (const auto& l : d["lines"]) {
        pdf::Line pl;
        pl.ref = l["sku"].is_null() ? "" : l["sku"].get<std::string>();
        pl.description = l["description"];
        std::string unit = l["unit"];
        std::string unitLabel = t("unit." + unit);
        pl.qty = fmtQty(l["quantity"]) + " " + (unitLabel == "unit." + unit ? unit : unitLabel);
        pl.unitPrice = fmtMoney(l["unit_price"]);
        pl.discount = l["discount_bp"].get<int>() > 0 ? percent(l["discount_bp"], lang) : "";
        pl.tax = percent(l["tax_rate_bp"], lang);
        pl.total = fmtMoney(l["net"]);
        v.lines.push_back(pl);
    }
    if (d["discount"] != money(0, cur)) {
        v.totals.push_back({t("doc.subtotal"), fmtMoney(d["gross"])});
        v.totals.push_back({t("doc.discount"), "-" + fmtMoney(d["discount"])});
    }
    v.totals.push_back({t("doc.net"), fmtMoney(d["net"])});
    for (const auto& tb : d["taxes"])
        v.totals.push_back({percent(tb["rate_bp"], lang) + " " + t("doc.tax_on") + " " + fmtMoney(tb["base"]),
                            fmtMoney(tb["tax"])});
    v.grandTotal = {t("doc.total"), fmtMoney(d["total"]) + " " + cur};
    if (d["doc_type"] == "invoice" && !draft) {
        if (d["paid"] != money(0, cur)) v.afterTotals.push_back({t("doc.paid"), fmtMoney(d["paid"])});
        if (d["credited"] != money(0, cur)) v.afterTotals.push_back({t("doc.credited"), fmtMoney(d["credited"])});
        v.afterTotals.push_back({t("doc.balance"), fmtMoney(d["balance"]) + " " + cur});
    }
    if (seller.value("vat_registered", false) && profile.contains("document_notices") &&
        profile["document_notices"].contains("vat_registered"))
        v.notices.push_back(profile["document_notices"]["vat_registered"].value(lang, ""));
    if (!d["fiscal_reference"].is_null()) {
        std::string fr;
        for (auto it = d["fiscal_reference"].begin(); it != d["fiscal_reference"].end(); ++it)
            if (it.value().is_string()) fr += (fr.empty() ? "" : " · ") + it.value().get<std::string>();
        v.notices.push_back(t("doc.fiscal_reference") + " : " + fr);
    }
    if (!d["terms"].get<std::string>().empty()) v.footnotes.push_back({t("doc.payment_terms"), d["terms"]});
    if (!seller.value("payment_instructions", "").empty())
        v.footnotes.push_back({t("doc.payment_info"), seller.value("payment_instructions", "")});
    if (!d["notes"].get<std::string>().empty()) v.footnotes.push_back({t("doc.notes"), d["notes"]});
    std::string fingerprint = crypto::sha256Hex(d["id"].get<std::string>() + "|" + v.number + "|" + d["issue_date"].get<std::string>() +
                                                "|" + d["total"].get<std::string>() + "|" + cur + "|" + v.buyerName)
                                  .substr(0, 16);
    v.footerLeft = seller.value("footer", "");
    v.footerLeft += (v.footerLeft.empty() ? "" : "  ·  ") + t("doc.generated_by") + "  ·  " + t("doc.fingerprint") + " " + fingerprint;
    v.pageLabel = t("doc.page");
    v.creationDateIso = d["issued_at"].is_null() ? d["updated_at"].get<std::string>() : d["issued_at"].get<std::string>();
    return pdf::renderSalesDocument(v, pdf::Fonts::fromDir(env.cfg.fontDir));
}

// --------------------------------------------------------------------- routes

namespace sales {

json issue(Env& env, db::Tx& tx, const Actor& actor, const std::string& id, const json& b,
           const std::optional<std::string>& warehouseId) {
    auto d = tx.one(
        "SELECT doc_type, status, currency, main_currency, issue_date::text, party_id, total_minor, "
        "related_document_id, restock, language, due_date::text FROM sales_documents WHERE id = $1 FOR UPDATE",
        id);
    std::string type = db::str(d[0]);
    if (db::str(d[1]) != "draft") throw conflict("already_issued");
    // Monthly quota of the plan; credit notes (corrections, returns) are never blocked.
    if (type != "credit_note") entitlements::requireCapacity(tx, actor, entitlements::key::kDocumentsMonth);
    if (tx.one("SELECT count(*) FROM sales_document_lines WHERE document_id = $1", id)[0].as<int>() == 0)
        throw invalid("lines", "at_least_one_line");
    const std::string cur = db::str(d[2]), mainCur = db::str(d[3]), issueDate = db::str(d[4]);
    const std::string lang = db::str(d[9]);
    const int64_t total = d[6].as<int64_t>();

    // Exchange rate snapshot for foreign-currency documents.
    std::optional<std::string> fxRate, fxDate, fxSource;
    if (cur != mainCur) {
        if (auto r = in::optStr(b, "fx_rate", 30)) {
            auto er = nhill::ExchangeRate::parse(*r);
            if (!er) throw invalid("fx_rate", "invalid_rate");
            fxRate = er->toString();
            fxDate = in::optDate(b, "fx_rate_date").value_or(issueDate);
            fxSource = in::optStr(b, "fx_rate_source", 100).value_or("manual");
        } else if (auto r = tx.opt("SELECT rate::text, rate_date::text, source FROM exchange_rates WHERE "
                                   "from_currency = $1 AND to_currency = $2 AND rate_date <= $3::date "
                                   "ORDER BY rate_date DESC LIMIT 1",
                                   cur, mainCur, issueDate)) {
            fxRate = db::str((*r)[0]);
            fxDate = db::str((*r)[1]);
            fxSource = db::str((*r)[2]);
        } else {
            throw invalid("fx_rate", "fx_rate_required");
        }
    }

    // Credit note: checks against the original invoice.
    std::optional<pqxx::row> inv;
    if (type == "credit_note") {
        inv = tx.one("SELECT status, currency, total_minor, paid_minor, credited_minor FROM sales_documents "
                     "WHERE id = $1 FOR UPDATE",
                     db::str(d[7]));
        std::string ist = db::str((*inv)[0]);
        if (ist != "issued" && ist != "partially_paid" && ist != "paid") throw conflict("invoice_not_creditable");
        if (db::str((*inv)[1]) != cur) throw invalid("currency", "must_match_invoice");
        if (total > (*inv)[2].as<int64_t>() - (*inv)[4].as<int64_t>())
            throw invalid("lines", "credit_exceeds_invoice");
    }

    // Snapshots
    auto e = tx.one(
        "SELECT e.trade_name, e.legal_name, e.address, e.city, e.country_code, e.phone, e.email, "
        "e.vat_registered, e.compliance_profile_code, s.payment_instructions, s.invoice_footer, "
        "s.default_payment_terms_days FROM establishments e JOIN establishment_settings s ON "
        "s.establishment_id = e.id WHERE e.id = $1",
        actor.establishmentId);
    const json& profile = env.profiles.get(db::str(e[8]));
    json ids = json::object();
    for (const auto& r : tx.q("SELECT kind, value FROM establishment_identifiers WHERE establishment_id = $1",
                              actor.establishmentId))
        ids[db::str(r[0])] = db::str(r[1]);
    json seller = {{"trade_name", db::str(e[0])}, {"legal_name", db::str(e[1])}, {"address", db::str(e[2])},
                   {"city", db::str(e[3])},       {"country", db::str(e[4])},    {"phone", db::str(e[5])},
                   {"email", db::str(e[6])},      {"vat_registered", db::boolean(e[7])},
                   {"profile", db::str(e[8])},    {"payment_instructions", db::str(e[9])},
                   {"footer", db::str(e[10])},    {"legal_ids", ids}};
    auto p = tx.one("SELECT name, address, city, country_code, email, phone, tax_ids, payment_terms_days, active "
                    "FROM parties WHERE id = $1",
                    db::str(d[5]));
    if (!db::boolean(p[8])) throw invalid("party_id", "inactive_customer");
    json buyer = {{"name", db::str(p[0])},  {"address", db::str(p[1])}, {"city", db::str(p[2])},
                  {"country", db::str(p[3])}, {"email", db::str(p[4])}, {"phone", db::str(p[5])},
                  {"tax_ids", json::parse(db::str(p[6]))}};
    int terms = p[7].as<int>() > 0 ? p[7].as<int>() : e[11].as<int>();

    // Gap-free, duplicate-free numbering: the sequence row is locked
    // until this transaction commits.
    auto seq = tx.one(
        "INSERT INTO document_sequences (establishment_id, doc_type, period_key, next_value) "
        "VALUES ($1, $2, $3, 2) ON CONFLICT (establishment_id, doc_type, period_key) "
        "DO UPDATE SET next_value = document_sequences.next_value + 1 RETURNING next_value - 1",
        actor.establishmentId, type, issueDate.substr(0, 4))[0].as<int64_t>();
    std::string number = formatNumber(profile, lang, type, issueDate.substr(0, 4), seq);
    std::string newStatus = type == "quote" ? "sent" : type == "order" ? "confirmed" : "issued";

    tx.q("UPDATE sales_documents SET status = $2, number = $3, sequence_value = $4, seller_snapshot = $5::jsonb, "
         "buyer_snapshot = $6::jsonb, profile_code = $7, profile_version = $8, fx_rate = $9::numeric, "
         "fx_rate_date = $10::date, fx_rate_source = $11, due_date = CASE WHEN doc_type = 'invoice' THEN "
         "COALESCE(due_date, issue_date + $12::int) ELSE due_date END, issued_by = $13, issued_at = now() WHERE id = $1",
         id, newStatus, number, seq, seller.dump(), buyer.dump(), db::str(e[8]), profile.at("version").get<int>(),
         fxRate, fxDate, fxSource, terms, actor.userId);
    history(tx, actor, id, std::string("draft"), newStatus);

    // Stock effects
    if (type == "invoice" || type == "receipt" || (type == "credit_note" && db::boolean(d[8]))) {
        std::string wh = warehouseId ? *warehouseId : db::str(tx.one("SELECT id FROM warehouses WHERE is_default")[0]);
        for (const auto& l : tx.q("SELECT l.product_id, l.quantity_milli FROM sales_document_lines l "
                                  "JOIN products p ON p.id = l.product_id WHERE l.document_id = $1 AND p.track_stock",
                                  id)) {
            int64_t q = l[1].as<int64_t>();
            tx.q("INSERT INTO stock_movements (establishment_id, product_id, warehouse_id, quantity_milli, kind, "
                 "source_type, source_id, note, created_by) VALUES ($1,$2,$3,$4,$5,'sales_document',$6,$7,$8)",
                 actor.establishmentId, db::str(l[0]), wh, type == "credit_note" ? q : -q,
                 type == "credit_note" ? "return_in" : "sale", id, number, actor.userId);
        }
    }
    // Credit applied to the original invoice
    if (type == "credit_note") {
        int64_t credited = (*inv)[4].as<int64_t>() + total;
        std::string st = invoiceStatusFor((*inv)[2].as<int64_t>(), (*inv)[3].as<int64_t>(), credited);
        tx.q("UPDATE sales_documents SET credited_minor = $2, status = $3 WHERE id = $1", db::str(d[7]),
             credited, st);
        if (st != db::str((*inv)[0])) history(tx, actor, db::str(d[7]), db::str((*inv)[0]), st, number);
    }
    std::string pdf = renderSalesPdf(env, tx, id);
    tx.q("UPDATE sales_documents SET pdf_sha256 = $2 WHERE id = $1", id, crypto::sha256Hex(pdf));
    audit(tx, actor, "sales_document.issued", "sales_document", id, {{"number", number}, {"type", type}});
    return loadSalesDocument(tx, id);
}

}  // namespace sales

namespace {

// A document of a feature outside the plan does not exist for the user (404).
void requireVisible(db::Tx& tx, const Actor& a, const std::string& id) {
    auto r = tx.opt("SELECT doc_type, pos_session_id IS NOT NULL FROM sales_documents WHERE id = $1", id);
    if (!r || !entitlements::docVisible(a, db::str((*r)[0]), db::boolean((*r)[1]))) throw notFound("document");
}

}  // namespace

void registerSalesRoutes(App& app, Env& env) {
    app.get("/api/v1/sales-documents", {Access::User}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto type = ctx.query("type");
        if (type && !kTypes.count(*type)) throw badRequest("invalid_type");
        auto status = ctx.query("status");
        auto party = ctx.query("party_id");
        if (party && !in::isUuid(*party)) party.reset();
        auto from = ctx.query("from"), to = ctx.query("to");
        if (from && !in::isDate(*from)) from.reset();
        if (to && !in::isDate(*to)) to.reset();
        auto search = ctx.query("search");
        bool overdue = ctx.query("overdue").value_or("") == "1";
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            std::string where =
                "WHERE ($1::text IS NULL OR d.doc_type = $1) AND ($2::text IS NULL OR d.status = $2) "
                "AND ($3::uuid IS NULL OR d.party_id = $3) AND ($4::date IS NULL OR d.issue_date >= $4::date) "
                "AND ($5::date IS NULL OR d.issue_date <= $5::date) AND ($6::text IS NULL OR d.number ILIKE '%' || $6 || '%' "
                "OR p.name ILIKE '%' || $6 || '%') ";
            if (overdue) where += "AND d.doc_type = 'invoice' AND d.status IN ('issued','partially_paid') AND d.due_date < nh_local_today() ";
            const std::string types = entitlements::docTypesArray(ctx.actor);
            int total = tx.one("SELECT count(*) FROM sales_documents d JOIN parties p ON p.id = d.party_id " + where + "AND " +
                                   entitlements::docVisibleSql("d", 7),
                               type, status, party, from, to, search, types)[0].as<int>();
            json items = json::array();
            for (const auto& r : tx.q(
                     "SELECT d.id, d.doc_type, d.status, d.number, p.name, d.issue_date::text, d.due_date::text, d.currency, "
                     "d.total_minor, d.paid_minor, d.credited_minor, d.party_id FROM sales_documents d "
                     "JOIN parties p ON p.id = d.party_id " + where + "AND " + entitlements::docVisibleSql("d", 9) +
                         " ORDER BY d.issue_date DESC, d.created_at DESC LIMIT $7 OFFSET $8",
                     type, status, party, from, to, search, pg.limit, pg.offset, types)) {
                std::string cur = db::str(r[7]);
                int64_t bal = r[8].as<int64_t>() - r[9].as<int64_t>() - r[10].as<int64_t>();
                items.push_back({{"id", db::str(r[0])},
                                 {"doc_type", db::str(r[1])},
                                 {"status", db::str(r[2])},
                                 {"number", r[3].is_null() ? json(nullptr) : json(db::str(r[3]))},
                                 {"party_name", db::str(r[4])},
                                 {"party_id", db::str(r[11])},
                                 {"issue_date", db::str(r[5])},
                                 {"due_date", r[6].is_null() ? json(nullptr) : json(db::str(r[6]))},
                                 {"currency", cur},
                                 {"total", money(r[8].as<int64_t>(), cur)},
                                 {"balance", money(bal, cur)}});
            }
            return {{"items", items}, {"total", total}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/sales-documents/:id", {Access::User}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "document");
        ctx.json(200, db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
                     requireVisible(tx, ctx.actor, id);
                     return loadSalesDocument(tx, id);
                 }));
    });

    app.get("/api/v1/sales-documents/:id/pdf", {Access::User}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "document");
        std::string number;
        std::string bytes = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            requireVisible(tx, ctx.actor, id);
            auto r = tx.one("SELECT COALESCE(number, 'brouillon') FROM sales_documents WHERE id = $1", id);
            number = db::str(r[0]);
            return renderSalesPdf(env, tx, id);
        });
        ctx.file(bytes, "application/pdf", number + ".pdf", ctx.query("inline").has_value());
    });

    // Create a draft (quote, order or invoice). Credit notes come from /credit-note.
    app.post("/api/v1/sales-documents", {Access::User, "sales.write", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string type = in::reqEnum(b, "doc_type", {"quote", "order", "invoice"});
        entitlements::requireFeature(ctx.actor, entitlements::featureOfDocClass(type));
        std::string party = in::reqUuid(b, "party_id");
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                auto p = tx.opt("SELECT is_customer, active FROM parties WHERE id = $1", party);
                if (!p || !db::boolean((*p)[0]) || !db::boolean((*p)[1])) throw invalid("party_id", "not_an_active_customer");
                auto es = tx.one("SELECT e.main_currency, s.document_language, s.tax_rounding FROM establishments e "
                                 "JOIN establishment_settings s ON s.establishment_id = e.id WHERE e.id = $1",
                                 ctx.actor.establishmentId);
                std::string currency = b.contains("currency") ? in::reqCurrency(b, "currency") : db::str(es[0]);
                if (!tx.opt("SELECT 1 FROM currencies WHERE code = $1 AND active", currency))
                    throw invalid("currency", "unsupported_currency");
                entitlements::requireCurrency(tx, ctx.actor, currency);
                std::string lang = in::optEnum(b, "language", {"fr", "en"}).value_or(db::str(es[1]));
                std::string rounding = db::str(es[2]);
                auto [lines, totals] = priceLines(tx, b, currency, rounding);
                std::string id = db::str(tx.one(
                    "INSERT INTO sales_documents (establishment_id, doc_type, party_id, currency, language, issue_date, "
                    "due_date, valid_until, main_currency, tax_rounding, gross_minor, discount_minor, net_minor, tax_minor, "
                    "total_minor, tax_breakdown, notes, terms, created_by) VALUES ($1,$2,$3,$4,$5,COALESCE($6::date, "
                    "nh_local_today()),$7,$8,$9,$10,$11,$12,$13,$14,$15,$16::jsonb,$17,$18,$19) RETURNING id",
                    ctx.actor.establishmentId, type, party, currency, lang, in::optDate(b, "issue_date"),
                    in::optDate(b, "due_date"), in::optDate(b, "valid_until"), db::str(es[0]), rounding, totals.grossMinor,
                    totals.discountMinor, totals.netMinor, totals.taxMinor, totals.totalMinor,
                    breakdownJson(totals).dump(), in::optStr(b, "notes", 2000).value_or(""),
                    in::optStr(b, "terms", 1000).value_or(""), ctx.actor.userId)[0]);
                writeLines(tx, ctx.actor.establishmentId, id, lines, totals);
                history(tx, ctx.actor, id, std::nullopt, "draft");
                audit(tx, ctx.actor, "sales_document.created", "sales_document", id, {{"doc_type", type}});
                return {201, loadSalesDocument(tx, id)};
            });
        });
        ctx.json(status, body);
    });

    // Replace a draft entirely.
    app.put("/api/v1/sales-documents/:id", {Access::User, "sales.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "document");
        const auto& b = ctx.body();
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            requireVisible(tx, ctx.actor, id);
            auto d = tx.one("SELECT status, currency, tax_rounding, party_id, doc_type FROM sales_documents WHERE id = $1 "
                            "FOR UPDATE",
                            id);
            if (db::str(d[0]) != "draft") throw conflict("document_immutable", "only drafts can be edited");
            std::string party = b.contains("party_id") ? in::reqUuid(b, "party_id") : db::str(d[3]);
            auto p = tx.opt("SELECT is_customer, active FROM parties WHERE id = $1", party);
            if (!p || !db::boolean((*p)[0])) throw invalid("party_id", "not_an_active_customer");
            std::string currency = b.contains("currency") ? in::reqCurrency(b, "currency") : db::str(d[1]);
            if (db::str(d[4]) == "credit_note" && currency != db::str(d[1])) throw invalid("currency", "immutable");
            if (currency != db::str(d[1])) entitlements::requireCurrency(tx, ctx.actor, currency);
            auto [lines, totals] = priceLines(tx, b, currency, db::str(d[2]));
            tx.q("UPDATE sales_documents SET party_id = $2, currency = $3, language = COALESCE($4, language), "
                 "issue_date = COALESCE($5::date, issue_date), due_date = $6, valid_until = $7, gross_minor = $8, "
                 "discount_minor = $9, net_minor = $10, tax_minor = $11, total_minor = $12, tax_breakdown = $13::jsonb, "
                 "notes = $14, terms = $15, restock = COALESCE($16, restock) WHERE id = $1",
                 id, party, currency, in::optEnum(b, "language", {"fr", "en"}), in::optDate(b, "issue_date"),
                 in::optDate(b, "due_date"), in::optDate(b, "valid_until"), totals.grossMinor, totals.discountMinor,
                 totals.netMinor, totals.taxMinor, totals.totalMinor, breakdownJson(totals).dump(),
                 in::optStr(b, "notes", 2000).value_or(""), in::optStr(b, "terms", 1000).value_or(""),
                 b.contains("restock") ? std::optional<bool>(in::optBool(b, "restock", false)) : std::nullopt);
            writeLines(tx, ctx.actor.establishmentId, id, lines, totals);
            audit(tx, ctx.actor, "sales_document.updated", "sales_document", id);
            return loadSalesDocument(tx, id);
        });
        ctx.json(200, out);
    });

    app.del("/api/v1/sales-documents/:id", {Access::User, "sales.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "document");
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            requireVisible(tx, ctx.actor, id);
            auto d = tx.one("SELECT status, related_document_id FROM sales_documents WHERE id = $1 FOR UPDATE", id);
            if (db::str(d[0]) != "draft") throw conflict("document_immutable", "issued documents are never deleted");
            tx.q("DELETE FROM sales_documents WHERE id = $1", id);
            audit(tx, ctx.actor, "sales_document.draft_deleted", "sales_document", id);
        });
        ctx.noContent();
    });

    // Issue: numbering, snapshots, stock, credit application, PDF fingerprint.
    app.post("/api/v1/sales-documents/:id/issue", {Access::User, "sales.issue", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "document");
        const auto& b = ctx.body();
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                requireVisible(tx, ctx.actor, id);
                auto t = tx.one("SELECT doc_type FROM sales_documents WHERE id = $1", id);
                if (db::str(t[0]) == "receipt") throw conflict("pos_document", "receipts are issued by Nhill POS");
                return {200, sales::issue(env, tx, ctx.actor, id, b)};
            });
        });
        ctx.json(status, body);
    });

    // Quote acceptance / refusal, order cancellation.
    app.post("/api/v1/sales-documents/:id/status", {Access::User, "sales.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "document");
        std::string to = in::reqEnum(ctx.body(), "status", {"accepted", "declined", "canceled"});
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            requireVisible(tx, ctx.actor, id);
            auto d = tx.one("SELECT doc_type, status FROM sales_documents WHERE id = $1 FOR UPDATE", id);
            std::string type = db::str(d[0]), from = db::str(d[1]);
            bool ok = (type == "quote" && from == "sent" && (to == "accepted" || to == "declined")) ||
                      (type == "order" && from == "confirmed" && to == "canceled");
            if (!ok) throw conflict("invalid_transition", "transition not allowed", {{"from", from}, {"to", to}});
            tx.q("UPDATE sales_documents SET status = $2 WHERE id = $1", id, to);
            history(tx, ctx.actor, id, from, to);
            audit(tx, ctx.actor, "sales_document.status_changed", "sales_document", id, {{"from", from}, {"to", to}});
            return loadSalesDocument(tx, id);
        });
        ctx.json(200, out);
    });

    // Quote -> order/invoice, order -> invoice. Creates a new draft.
    app.post("/api/v1/sales-documents/:id/convert", {Access::User, "sales.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "document");
        std::string to = in::reqEnum(ctx.body(), "to", {"order", "invoice"});
        entitlements::requireFeature(ctx.actor, entitlements::featureOfDocClass(to));
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                requireVisible(tx, ctx.actor, id);
                auto d = tx.one("SELECT doc_type, status, party_id, currency, language, notes, terms, tax_rounding, "
                                "main_currency, gross_minor, discount_minor, net_minor, tax_minor, total_minor, tax_breakdown "
                                "FROM sales_documents WHERE id = $1 FOR UPDATE",
                                id);
                std::string type = db::str(d[0]), st = db::str(d[1]);
                bool ok = (type == "quote" && (st == "sent" || st == "accepted")) || (type == "order" && st == "confirmed" && to == "invoice");
                if (!ok) throw conflict("invalid_transition", "this document cannot be converted");
                std::string nid = db::str(tx.one(
                    "INSERT INTO sales_documents (establishment_id, doc_type, party_id, currency, language, main_currency, "
                    "tax_rounding, gross_minor, discount_minor, net_minor, tax_minor, total_minor, tax_breakdown, notes, terms, "
                    "related_document_id, created_by) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13::jsonb,$14,$15,$16,$17) "
                    "RETURNING id",
                    ctx.actor.establishmentId, to, db::str(d[2]), db::str(d[3]), db::str(d[4]), db::str(d[8]), db::str(d[7]),
                    d[9].as<int64_t>(), d[10].as<int64_t>(), d[11].as<int64_t>(), d[12].as<int64_t>(), d[13].as<int64_t>(),
                    db::str(d[14]), db::str(d[5]), db::str(d[6]), id, ctx.actor.userId)[0]);
                tx.q("INSERT INTO sales_document_lines (establishment_id, document_id, line_no, product_id, description, "
                     "quantity_milli, unit, unit_price_minor, discount_bp, tax_rate_bp, tax_code, gross_minor, discount_minor, "
                     "net_minor, tax_minor, total_minor) SELECT establishment_id, $2, line_no, product_id, description, "
                     "quantity_milli, unit, unit_price_minor, discount_bp, tax_rate_bp, tax_code, gross_minor, discount_minor, "
                     "net_minor, tax_minor, total_minor FROM sales_document_lines WHERE document_id = $1",
                     id, nid);
                tx.q("UPDATE sales_documents SET status = 'converted' WHERE id = $1", id);
                history(tx, ctx.actor, id, st, "converted");
                history(tx, ctx.actor, nid, std::nullopt, "draft");
                audit(tx, ctx.actor, "sales_document.converted", "sales_document", id, {{"to", to}, {"new_id", nid}});
                return {201, loadSalesDocument(tx, nid)};
            });
        });
        ctx.json(status, body);
    });

    // Draft credit note for an issued invoice (all lines by default).
    app.post("/api/v1/sales-documents/:id/credit-note", {Access::User, "sales.issue", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "document");
        const auto& b = ctx.body();
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                requireVisible(tx, ctx.actor, id);
                entitlements::requireFeature(ctx.actor, entitlements::key::kInvoices);
                auto d = tx.one("SELECT doc_type, status, party_id, currency, language, tax_rounding, main_currency "
                                "FROM sales_documents WHERE id = $1",
                                id);
                if (db::str(d[0]) != "invoice" || db::str(d[1]) == "draft" || db::str(d[1]) == "credited")
                    throw conflict("invoice_not_creditable");
                json lines = json::array();
                if (b.contains("lines")) {
                    lines = b["lines"];
                } else {
                    for (const auto& l : tx.q("SELECT product_id, description, quantity_milli, unit, unit_price_minor, "
                                              "discount_bp, tax_rate_bp, tax_code FROM sales_document_lines WHERE document_id = $1 "
                                              "ORDER BY line_no",
                                              id)) {
                        json lj = {{"description", db::str(l[1])}, {"quantity", qty(l[2].as<int64_t>())},
                                   {"unit", db::str(l[3])}, {"unit_price", money(l[4].as<int64_t>(), db::str(d[3]))},
                                   {"discount_bp", l[5].as<int>()}};
                        if (!l[0].is_null()) lj["product_id"] = db::str(l[0]);
                        auto tr = tx.opt("SELECT id FROM tax_rates WHERE code = $1", db::str(l[7]));
                        if (tr) lj["tax_rate_id"] = db::str((*tr)[0]);
                        lines.push_back(lj);
                    }
                }
                json pb = {{"lines", lines}};
                auto [lc, totals] = priceLines(tx, pb, db::str(d[3]), db::str(d[5]));
                std::string nid = db::str(tx.one(
                    "INSERT INTO sales_documents (establishment_id, doc_type, party_id, currency, language, main_currency, "
                    "tax_rounding, gross_minor, discount_minor, net_minor, tax_minor, total_minor, tax_breakdown, notes, "
                    "related_document_id, restock, created_by) VALUES ($1,'credit_note',$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,"
                    "$12::jsonb,$13,$14,$15,$16) RETURNING id",
                    ctx.actor.establishmentId, db::str(d[2]), db::str(d[3]), db::str(d[4]), db::str(d[6]), db::str(d[5]),
                    totals.grossMinor, totals.discountMinor, totals.netMinor, totals.taxMinor, totals.totalMinor,
                    breakdownJson(totals).dump(), in::optStr(b, "reason", 500).value_or(""), id,
                    in::optBool(b, "restock", false), ctx.actor.userId)[0]);
                writeLines(tx, ctx.actor.establishmentId, nid, lc, totals);
                history(tx, ctx.actor, nid, std::nullopt, "draft");
                audit(tx, ctx.actor, "sales_document.credit_note_drafted", "sales_document", nid, {{"invoice", id}});
                return {201, loadSalesDocument(tx, nid)};
            });
        });
        ctx.json(status, body);
    });

    // Reference of the standardised invoice issued by the user's own DEF (RDC).
    app.put("/api/v1/sales-documents/:id/fiscal-reference", {Access::User, "sales.issue", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "document");
        const auto& b = ctx.body();
        json ref = {{"def_nid", in::reqStr(b, "def_nid", 60)},
                    {"def_auth_code", in::reqStr(b, "def_auth_code", 120)},
                    {"normalized_invoice_number", in::optStr(b, "normalized_invoice_number", 60).value_or("")}};
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            requireVisible(tx, ctx.actor, id);
            auto d = tx.one("SELECT doc_type, status FROM sales_documents WHERE id = $1 FOR UPDATE", id);
            if (db::str(d[1]) == "draft" || (db::str(d[0]) != "invoice" && db::str(d[0]) != "credit_note"))
                throw conflict("not_an_issued_invoice");
            tx.q("UPDATE sales_documents SET fiscal_reference = $2::jsonb WHERE id = $1", id, ref.dump());
            audit(tx, ctx.actor, "sales_document.fiscal_reference_recorded", "sales_document", id, ref);
            return loadSalesDocument(tx, id);
        });
        ctx.json(200, out);
    });
}

}  // namespace nh
