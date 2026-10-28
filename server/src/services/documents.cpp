#include "services/documents.hpp"

#include "nhill/money.hpp"
#include "services/sales.hpp"
#include "util/crypto.hpp"

namespace nh::documents {

namespace {

std::string nbsp(std::string s) {
    // The PDF fonts carry U+00A0 for sure; the narrow no-break space is normalised.
    for (size_t p; (p = s.find("\xE2\x80\xAF")) != std::string::npos;) s.replace(p, 3, "\xC2\xA0");
    return s;
}

std::string percent(int bp, const std::string& lang) {
    std::string s = nhill::formatDecimal(bp, 2, lang, true);
    return lang == "fr" ? s + "\xC2\xA0%" : s + "%";
}

void addIf(std::vector<std::string>& v, const std::string& s) {
    if (!s.empty()) v.push_back(s);
}

}  // namespace

Model build(Env& env, db::Tx& tx, const std::string& id) {
    json d = loadSalesDocument(tx, id);
    Model m;
    m.id = id;
    m.docType = d["doc_type"];
    m.status = d["status"];
    m.lang = d["language"];
    m.currency = d["currency"];
    const std::string& lang = m.lang;
    const std::string& cur = m.currency;
    auto t = [&](const std::string& k) { return env.i18n.t(lang, k); };
    auto has = [&](const std::string& k) { return env.i18n.t(lang, k) != k; };
    auto fmtMoney = [&](const std::string& plain) {
        return nbsp(nhill::formatDecimal(nhill::parseMoney(plain, minorUnits(cur)).value_or(0), minorUnits(cur), lang));
    };
    auto fmtQty = [&](const std::string& plain) {
        return nbsp(nhill::formatDecimal(nhill::parseQuantity(plain).value_or(0), 3, lang, true));
    };

    auto row = tx.one(
        "SELECT d.seller_snapshot, d.buyer_snapshot, e.trade_name, e.legal_name, e.address, e.city, e.country_code, "
        "e.phone, e.email, e.vat_registered, e.compliance_profile_code, s.payment_instructions, s.invoice_footer, "
        "p.name, p.address, p.city, p.country_code, p.email, p.phone, p.tax_ids, o.mime, encode(o.data,'hex'), "
        "s.document_mentions, d.pos_session_id IS NOT NULL, "
        "to_char(COALESCE(d.issued_at, d.created_at) AT TIME ZONE nh_tz(d.establishment_id), 'HH24:MI'), "
        "COALESCE(u.full_name, '') "
        "FROM sales_documents d JOIN establishments e ON e.id = d.establishment_id "
        "JOIN establishment_settings s ON s.establishment_id = d.establishment_id JOIN parties p ON p.id = d.party_id "
        "LEFT JOIN objects o ON o.id = e.logo_object_id LEFT JOIN users u ON u.id = d.issued_by WHERE d.id = $1",
        id);
    m.draft = m.status == "draft";
    m.pos = db::boolean(row[23]);
    json seller, buyer;
    if (!m.draft && !row[0].is_null()) {
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

    // Till details (documents of a cash session).
    std::optional<pqxx::row> till;
    if (m.pos) {
        till = tx.one("SELECT s.number, r.code, r.name, COALESCE(ps.receipt_footer, ''), COALESCE(ps.receipt_width_mm, 80), "
                      "COALESCE(ps.walk_in_party_id = d.party_id, false) FROM sales_documents d "
                      "JOIN pos_sessions s ON s.id = d.pos_session_id JOIN pos_registers r ON r.id = s.register_id "
                      "LEFT JOIN pos_settings ps ON ps.establishment_id = d.establishment_id WHERE d.id = $1",
                      id);
        m.walkIn = db::boolean((*till)[5]);
        m.ticketWidthMm = static_cast<float>((*till)[4].as<int>());
        m.closing = db::str((*till)[3]);
    }

    // ---- identity
    m.title = t("doc." + m.docType);
    m.number = d["number"].is_null() ? "" : d["number"].get<std::string>();
    if (m.draft) m.watermark = t("doc.draft_watermark");
    m.sellerName = seller.value("trade_name", "");
    if (seller.value("legal_name", "") != m.sellerName) addIf(m.sellerLines, seller.value("legal_name", ""));
    addIf(m.sellerLines, seller.value("address", ""));
    addIf(m.sellerLines, seller.value("city", ""));
    std::string contact = seller.value("phone", "");
    if (!seller.value("email", "").empty()) contact += (contact.empty() ? "" : " · ") + seller.value("email", "");
    addIf(m.sellerLines, contact);
    std::string ids;
    for (const auto& def : profile.at("legal_ids")) {
        std::string k = def.at("kind");
        if (seller.contains("legal_ids") && seller["legal_ids"].contains(k))
            ids += (ids.empty() ? "" : " · ") + t("id." + k) + " " + seller["legal_ids"][k].get<std::string>();
    }
    addIf(m.sellerLines, ids);
    if (!row[20].is_null()) {
        m.logoBytes = *crypto::fromHex(db::str(row[21]));
        m.logoMime = db::str(row[20]);
    }
    if (!m.walkIn) {
        m.buyerLabel = t(m.docType == "quote" || m.docType == "order" ? "doc.customer" : "doc.bill_to");
        m.buyerName = buyer.value("name", "");
        addIf(m.buyerLines, buyer.value("address", ""));
        addIf(m.buyerLines, buyer.value("city", ""));
        std::string bids;
        if (buyer.contains("tax_ids"))
            for (auto it = buyer["tax_ids"].begin(); it != buyer["tax_ids"].end(); ++it)
                if (it.value().is_string())
                    bids += (bids.empty() ? "" : " · ") + t("id." + it.key()) + " " + it.value().get<std::string>();
        addIf(m.buyerLines, bids);
        std::string bcontact = buyer.value("phone", "");
        if (!buyer.value("email", "").empty()) bcontact += (bcontact.empty() ? "" : " · ") + buyer.value("email", "");
        addIf(m.buyerLines, bcontact);
    }

    // ---- meta
    // Receipts give the time of the sale; invoices keep the document date only, so that an
    // invoice is the same wherever it was issued.
    std::string date = formatDate(d["issue_date"], lang);
    if (m.docType == "receipt" || (m.pos && m.docType == "credit_note")) date += " " + db::str(row[24]);
    m.meta.push_back({t("doc.issue_date"), date});
    if (!d["due_date"].is_null() && m.docType == "invoice") m.meta.push_back({t("doc.due_date"), formatDate(d["due_date"], lang)});
    if (!d["valid_until"].is_null() && m.docType == "quote")
        m.meta.push_back({t("doc.valid_until"), formatDate(d["valid_until"], lang)});
    if (!d["related_number"].is_null()) m.meta.push_back({t("doc.related"), d["related_number"]});
    const std::string stateKey = "doc.state." + m.docType + "." + m.status;
    if (!m.draft && has(stateKey)) m.meta.push_back({t("doc.status"), t(stateKey)});
    m.meta.push_back({t("doc.currency"), cur});
    if (!d["fx_rate"].is_null()) {
        auto rate = nhill::ExchangeRate::parse(d["fx_rate"].get<std::string>());
        m.meta.push_back({t("doc.fx"), "1 " + cur + " = " + (rate ? rate->toString() : "") + " " +
                                           d["main_currency"].get<std::string>() + " (" +
                                           formatDate(d["fx_rate_date"].get<std::string>(), lang) + ")"});
    }
    if (till) {
        m.meta.push_back({t("pos.register"), db::str((*till)[1]) + " · " + t("pos.session") + " " +
                                                 std::to_string((*till)[0].as<int>())});
        if (!db::str(row[25]).empty()) m.meta.push_back({t("pos.cashier"), db::str(row[25])});
    }

    // ---- lines and totals (same rounding as the document: amounts come from the database)
    m.columns = {t("doc.col.ref"),      t("doc.col.description"), t("doc.col.qty"),  t("doc.col.unit_price"),
                 t("doc.col.discount"), t("doc.col.tax"),         t("doc.col.total")};
    for (const auto& l : d["lines"]) {
        pdf::Line pl;
        pl.ref = l["sku"].is_null() ? "" : l["sku"].get<std::string>();
        pl.description = l["description"];
        std::string unit = l["unit"];
        std::string unitLabel = t("unit." + unit);
        pl.qty = fmtQty(l["quantity"]);
        pl.unit = unitLabel == "unit." + unit ? unit : unitLabel;
        pl.unitPrice = fmtMoney(l["unit_price"]);
        pl.discount = l["discount_bp"].get<int>() > 0 ? percent(l["discount_bp"], lang) : "";
        pl.tax = percent(l["tax_rate_bp"], lang);
        pl.total = fmtMoney(l["net"]);
        m.lines.push_back(pl);
    }
    if (d["discount"] != money(0, cur)) {
        m.totals.push_back({t("doc.subtotal"), fmtMoney(d["gross"])});
        m.totals.push_back({t("doc.discount"), "-" + fmtMoney(d["discount"])});
    }
    m.totals.push_back({t("doc.net"), fmtMoney(d["net"])});
    for (const auto& tb : d["taxes"])
        m.totals.push_back({percent(tb["rate_bp"], lang) + " " + t("doc.tax_on") + " " + fmtMoney(tb["base"]),
                            fmtMoney(tb["tax"])});
    m.grandTotal = {t("doc.total"), fmtMoney(d["total"]) + " " + cur};

    // ---- payments (methods of Business and of the till, by their label)
    const bool refund = m.docType == "credit_note";
    m.paymentsTitle = t(refund ? "doc.refunds" : "doc.payments");
    for (const auto& p : tx.q(
             "SELECT y.method, COALESCE(pm.label, ''), y.amount_minor, "
             "to_char(y.paid_at AT TIME ZONE nh_tz(y.establishment_id), 'YYYY-MM-DD'), "
             // The till's internal reference (register/session/number) is not printed.
             "CASE WHEN y.note LIKE 'pos:%' THEN '' ELSE COALESCE(y.reference, '') END FROM payments y "
             "LEFT JOIN pos_payment_methods pm ON y.note LIKE 'pos:%' AND pm.code = substr(y.note, 5) "
             "WHERE y.document_id = $1 ORDER BY y.paid_at, y.created_at",
             id)) {
        std::string label = db::str(p[1]).empty() ? t("method." + db::str(p[0])) : db::str(p[1]);
        label += " · " + formatDate(db::str(p[3]), lang);
        if (!db::str(p[4]).empty()) label += " · " + db::str(p[4]);
        m.payments.push_back({label, fmtMoney(money(p[2].as<int64_t>(), cur))});
    }
    if ((m.docType == "invoice" || m.docType == "receipt") && !m.draft) {
        // A receipt is paid at the sale: its payments are listed above, only the change follows.
        if (m.docType == "invoice" && d["paid"] != money(0, cur)) m.afterTotals.push_back({t("doc.paid"), fmtMoney(d["paid"])});
        if (d["credited"] != money(0, cur)) m.afterTotals.push_back({t("doc.credited"), fmtMoney(d["credited"])});
        if (m.docType == "invoice" || d["balance"] != money(0, cur))
            m.afterTotals.push_back({t("doc.balance"), fmtMoney(d["balance"]) + " " + cur});
        if (m.pos) {
            auto change = tx.opt("SELECT (details->>'change_minor')::bigint FROM audit_log WHERE entity = 'sales_document' "
                                 "AND entity_id = $1 AND action = 'pos.sale' LIMIT 1",
                                 id);
            if (change && !(*change)[0].is_null() && (*change)[0].as<int64_t>() > 0)
                m.afterTotals.push_back({t("pos.change"), fmtMoney(money((*change)[0].as<int64_t>(), cur))});
        }
    }

    // ---- notices and footnotes
    if (seller.value("vat_registered", false) && profile.contains("document_notices") &&
        profile["document_notices"].contains("vat_registered"))
        m.notices.push_back(profile["document_notices"]["vat_registered"].value(lang, ""));
    if (!d["fiscal_reference"].is_null()) {
        std::string fr;
        for (auto it = d["fiscal_reference"].begin(); it != d["fiscal_reference"].end(); ++it)
            if (it.value().is_string()) fr += (fr.empty() ? "" : " · ") + it.value().get<std::string>();
        std::string label = t("doc.fiscal_reference");
        if (profile.contains("fiscal_device") && profile["fiscal_device"].contains("reference_label"))
            label = profile["fiscal_device"]["reference_label"].value(lang, label);
        m.notices.push_back(label + " : " + fr);
    }
    if (!db::str(row[22]).empty()) m.notices.push_back(db::str(row[22]));
    if (m.docType == "receipt") m.notices.push_back(t("pos.receipt_notice"));
    const bool payable = m.docType == "invoice" || m.docType == "quote" || m.docType == "order";
    if (payable && !d["terms"].get<std::string>().empty()) m.footnotes.push_back({t("doc.payment_terms"), d["terms"]});
    if (payable && !seller.value("payment_instructions", "").empty())
        m.footnotes.push_back({t("doc.payment_info"), seller.value("payment_instructions", "")});
    if (!d["notes"].get<std::string>().empty()) m.footnotes.push_back({t("doc.notes"), d["notes"]});

    std::string fingerprint = crypto::sha256Hex(d["id"].get<std::string>() + "|" + m.number + "|" +
                                                d["issue_date"].get<std::string>() + "|" + d["total"].get<std::string>() +
                                                "|" + cur + "|" + buyer.value("name", ""))
                                  .substr(0, 16);
    m.footer = seller.value("footer", "");
    m.generated = t("doc.generated_by") + "  ·  " + t("doc.fingerprint") + " " + fingerprint;
    if (m.docType == "receipt") m.closing += (m.closing.empty() ? "" : "\n") + t("pos.thanks");
    m.pageLabel = t("doc.page");
    m.creationDateIso = d["issued_at"].is_null() ? d["updated_at"].get<std::string>() : d["issued_at"].get<std::string>();
    return m;
}

std::string renderA4(Env& env, const Model& m) {
    pdf::SalesDocView v;
    v.title = m.title;
    v.number = m.number;
    v.watermark = m.watermark;
    v.sellerName = m.sellerName;
    v.sellerLines = m.sellerLines;
    v.buyerLabel = m.buyerLabel;
    v.buyerName = m.buyerName;
    v.buyerLines = m.buyerLines;
    v.logoBytes = m.logoBytes;
    v.logoMime = m.logoMime;
    v.meta = m.meta;
    v.columns = m.columns;
    for (auto l : m.lines) {
        l.qty += " " + l.unit;
        v.lines.push_back(l);
    }
    v.totals = m.totals;
    v.grandTotal = m.grandTotal;
    v.paymentsTitle = m.paymentsTitle;
    v.payments = m.payments;
    v.afterTotals = m.afterTotals;
    v.notices = m.notices;
    if (!m.closing.empty()) v.notices.push_back(m.closing);
    v.footnotes = m.footnotes;
    v.footerLeft = m.footer + (m.footer.empty() ? "" : "  ·  ") + m.generated;
    v.pageLabel = m.pageLabel;
    v.creationDateIso = m.creationDateIso;
    return pdf::renderSalesDocument(v, pdf::Fonts::fromDir(env.cfg.fontDir));
}

std::string renderTicket(Env& env, const Model& m, std::optional<float> widthMm) {
    using B = pdf::TicketBlock;
    pdf::TicketView v;
    v.widthMm = widthMm.value_or(m.ticketWidthMm);
    v.logoBytes = m.logoBytes;
    v.logoMime = m.logoMime;
    v.title = m.title + (m.number.empty() ? "" : " " + m.number);
    v.creationDateIso = m.creationDateIso;
    v.blocks.push_back({B::Title, m.sellerName});
    for (const auto& l : m.sellerLines) v.blocks.push_back({B::Center, l});
    v.blocks.push_back({B::Rule});
    if (!m.watermark.empty()) v.blocks.push_back({B::Center, m.watermark});
    v.blocks.push_back({B::KVStrong, m.title, m.number});
    for (const auto& [k, val] : m.meta) v.blocks.push_back({B::KV, k, val});
    if (!m.buyerName.empty()) {
        v.blocks.push_back({B::Rule});
        v.blocks.push_back({B::KV, m.buyerLabel, m.buyerName});
        for (const auto& l : m.buyerLines) v.blocks.push_back({B::Muted, l});
    }
    v.blocks.push_back({B::Rule});
    for (const auto& l : m.lines) {
        std::string detail = l.qty + " " + l.unit + " × " + l.unitPrice;
        if (!l.discount.empty()) detail += " (−" + l.discount + ")";
        detail += " · " + l.tax;
        v.blocks.push_back({B::Item, l.description, detail, l.total});
    }
    v.blocks.push_back({B::Rule});
    for (const auto& [k, val] : m.totals) v.blocks.push_back({B::KV, k, val});
    v.blocks.push_back({B::KVStrong, m.grandTotal.first, m.grandTotal.second});
    if (!m.payments.empty()) {
        v.blocks.push_back({B::Text, m.paymentsTitle});
        for (const auto& [k, val] : m.payments) v.blocks.push_back({B::KV, k, val});
    }
    for (const auto& [k, val] : m.afterTotals) v.blocks.push_back({B::KV, k, val});
    v.blocks.push_back({B::Rule});
    for (const auto& n : m.notices) v.blocks.push_back({B::Muted, n});
    for (const auto& [k, val] : m.footnotes) v.blocks.push_back({B::Muted, k + " : " + val});
    if (!m.closing.empty()) v.blocks.push_back({B::Center, m.closing});
    if (!m.footer.empty()) v.blocks.push_back({B::Center, m.footer});
    v.blocks.push_back({B::Space});
    v.blocks.push_back({B::Muted, m.generated});
    return pdf::renderTicket(v, pdf::Fonts::fromDir(env.cfg.fontDir));
}

std::string render(Env& env, const Model& m, const std::optional<std::string>& format, const std::optional<std::string>& width) {
    std::string f = format.value_or(m.docType == "receipt" ? "ticket" : "a4");
    if (f != "a4" && f != "ticket") throw invalid("format", "a4_or_ticket");
    std::optional<float> w;
    if (width) {
        if (*width != "58" && *width != "80") throw invalid("width", "58_or_80");
        w = static_cast<float>(std::stoi(*width));
    }
    return f == "a4" ? renderA4(env, m) : renderTicket(env, m, w);
}

json toJson(const Model& m) {
    auto kvs = [](const std::vector<pdf::KV>& v) {
        json a = json::array();
        for (const auto& [k, val] : v) a.push_back({k, val});
        return a;
    };
    json lines = json::array();
    for (const auto& l : m.lines)
        lines.push_back({l.ref, l.description, l.qty, l.unit, l.unitPrice, l.discount, l.tax, l.total});
    return {{"doc_type", m.docType}, {"status", m.status},      {"title", m.title},        {"number", m.number},
            {"pos", m.pos},          {"seller", m.sellerName},  {"seller_lines", m.sellerLines},
            {"buyer_label", m.buyerLabel}, {"buyer", m.buyerName}, {"buyer_lines", m.buyerLines},
            {"meta", kvs(m.meta)},   {"columns", m.columns},    {"lines", lines},          {"totals", kvs(m.totals)},
            {"grand_total", {m.grandTotal.first, m.grandTotal.second}}, {"payments_title", m.paymentsTitle},
            {"payments", kvs(m.payments)}, {"after_totals", kvs(m.afterTotals)}, {"notices", m.notices},
            {"footnotes", kvs(m.footnotes)}, {"footer", m.footer}, {"closing", m.closing}, {"generated", m.generated}};
}

}  // namespace nh::documents
