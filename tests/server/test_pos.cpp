// Nhill POS: registers and plan limits, roles and assignment, sales with several
// payments and change, supervisor approvals, returns, voids, cash movements,
// closing with variance, receipts and Z reports, tenant isolation.
#include "harness.hpp"

using namespace nht;

namespace {

Response post(const std::string& path, const json& body, const std::string& token) {
    return Harness::get().post(path, body, token, newKey());
}

struct Shop {
    Tenant t;
    std::string reg, cashier, cashierEmail, manager, managerEmail, p1, p2;
};

Shop shop() {
    Shop s;
    s.t = createTenant("company");
    auto r = post("/api/v1/pos/registers", {{"code", "c1"}, {"name", "Caisse 1"}, {"location", "Comptoir"}}, s.t.token);
    if (r.status != 201) throw std::runtime_error("register: " + r.raw);
    s.reg = r.body["id"];
    s.cashier = addMember(s.t, "cashier", &s.cashierEmail);
    s.manager = addMember(s.t, "cash_manager", &s.managerEmail);
    auto ids = Harness::get().su("SELECT u.id FROM users u WHERE u.email IN ('" + s.cashierEmail + "')");
    auto u = Harness::get().request("PUT", "/api/v1/pos/registers/" + s.reg + "/users", {{"user_ids", {ids[0][0].c_str()}}},
                                    s.t.token, newKey());
    if (u.status != 200) throw std::runtime_error("assign: " + u.raw);
    s.p1 = createProduct(s.t, uniq("SKU"), "1500.00");
    s.p2 = createProduct(s.t, uniq("SKU"), "2500.00");
    adjustStock(s.t, s.p1, "20");
    adjustStock(s.t, s.p2, "20");
    return s;
}

std::string openSession(const Shop& s, const std::string& token, const std::string& fl = "10000.00") {
    auto r = post("/api/v1/pos/sessions", {{"register_id", s.reg}, {"opening_float", fl}}, token);
    if (r.status != 201) throw std::runtime_error("open: " + r.raw);
    return r.body["id"];
}

std::string stockOf(const Tenant& /*t*/, const std::string& product) {
    return Harness::get().su("SELECT sum(quantity_milli) FROM stock_levels WHERE product_id = '" + product + "'")[0][0].c_str();
}

std::string approval(const Shop& s, const std::string& requester, const std::string& action, const std::string& session) {
    auto r = post("/api/v1/pos/approvals", {{"email", s.managerEmail}, {"password", "Membre-password-2026"}, {"action", action},
                                           {"session_id", session}},
                  requester);
    if (r.status != 201) throw std::runtime_error("approval: " + r.raw);
    return r.body["token"];
}

}  // namespace

TEST(Pos, RegistersAreLimitedByThePlan) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    ASSERT_EQ(post("/api/v1/pos/registers", {{"code", "C1"}, {"name", "Caisse 1"}}, t.token).status, 201);
    // Free trial: one register.
    auto r = post("/api/v1/pos/registers", {{"code", "C2"}, {"name", "Caisse 2"}}, t.token);
    EXPECT_EQ(r.status, 409);
    EXPECT_EQ(r.body["error"]["code"], "plan_register_limit");
    std::string plan = h.su("INSERT INTO plans (code, name_fr, name_en, billing_period, price_minor, currency, max_registers) "
                            "VALUES ('pos-" + uniq() + "', 'Multi', 'Multi', 'monthly', 1000, 'USD', 3) RETURNING id")[0][0].c_str();
    h.su("UPDATE subscriptions SET is_trial = false, status = 'active', plan_id = '" + plan + "' WHERE establishment_id = '" + t.id + "'");
    EXPECT_EQ(post("/api/v1/pos/registers", {{"code", "C2"}, {"name", "Caisse 2"}}, t.token).status, 201);
    // Only managers configure registers.
    std::string cashier = addMember(t, "cashier");
    EXPECT_EQ(post("/api/v1/pos/registers", {{"code", "C3"}, {"name", "Caisse 3"}}, cashier).status, 403);
}

TEST(Pos, SaleWithSeveralPaymentsChangeStockAndReceipt) {
    auto& h = Harness::get();
    Shop s = shop();
    std::string sid = openSession(s, s.cashier);
    // Wrong totals are refused.
    auto bad = post("/api/v1/pos/sales", {{"session_id", sid}, {"lines", {{{"product_id", s.p1}, {"quantity", "2"}}}},
                                         {"payments", {{{"method", "cash"}, {"amount", "1000.00"}}}}},
                    s.cashier);
    EXPECT_EQ(bad.status, 422);
    EXPECT_EQ(bad.body["error"]["details"]["reason"], "payments_must_equal_total");
    // Card method configured by the owner.
    ASSERT_EQ(post("/api/v1/pos/payment-methods", {{"code", "card"}, {"label", "Carte"}, {"kind", "card"}}, s.t.token).status, 201);
    auto sale = post("/api/v1/pos/sales",
                     {{"session_id", sid},
                      {"lines", {{{"product_id", s.p1}, {"quantity", "2"}}, {{"product_id", s.p2}, {"quantity", "1"}}}},
                      {"payments", {{{"method", "card"}, {"amount", "2500.00"}},
                                    {{"method", "cash"}, {"amount", "3000.00"}, {"tendered", "5000.00"}}}}},
                     s.cashier);
    ASSERT_EQ(sale.status, 201) << sale.raw;
    EXPECT_EQ(sale.body["change"], "2000.00");
    const json& rc = sale.body["receipt"];
    EXPECT_EQ(rc["doc_type"], "receipt");
    EXPECT_EQ(rc["status"], "paid");
    EXPECT_EQ(rc["total"], "5500.00");
    EXPECT_EQ(rc["number"].get<std::string>().rfind("TK-", 0), 0u);
    EXPECT_EQ(rc["payments"].size(), 2u);
    EXPECT_EQ(stockOf(s.t, s.p1), "18000");
    // Receipt (roll paper) PDF.
    auto pdf = h.get("/api/v1/pos/sales/" + rc["id"].get<std::string>() + "/receipt.pdf", s.cashier);
    EXPECT_EQ(pdf.status, 200);
    EXPECT_EQ(pdf.raw.rfind("%PDF", 0), 0u);
    // The sale is visible to Nhill Business (same documents and payments).
    auto docs = h.get("/api/v1/sales-documents/" + rc["id"].get<std::string>(), s.t.token);
    EXPECT_EQ(docs.status, 200);
    auto sess = h.get("/api/v1/pos/sessions/" + sid, s.cashier);
    EXPECT_EQ(sess.body["sales_count"], 1);
    // A cashier never sees the expected cash (blind closing).
    EXPECT_FALSE(sess.body.contains("cash_expected"));
    EXPECT_TRUE(h.get("/api/v1/pos/sessions/" + sid, s.manager).body.contains("cash_expected"));
}

TEST(Pos, CashierIsLimitedToTheAssignedRegisterAndOwnSession) {
    auto& h = Harness::get();
    Shop s = shop();
    std::string other = addMember(s.t, "cashier");
    EXPECT_EQ(post("/api/v1/pos/sessions", {{"register_id", s.reg}}, other).status, 404);  // not assigned
    std::string sid = openSession(s, s.cashier);
    EXPECT_EQ(post("/api/v1/pos/sessions", {{"register_id", s.reg}}, s.manager).body["error"]["code"], "session_already_open");
    EXPECT_EQ(h.get("/api/v1/pos/sessions/" + sid, other).status, 404);
    EXPECT_EQ(post("/api/v1/pos/sales", {{"session_id", sid}, {"lines", {{{"product_id", s.p1}, {"quantity", "1"}}}},
                                        {"payments", {{{"method", "cash"}, {"amount", "1500.00"}}}}},
                   other).status, 404);
    // Another establishment cannot reach anything.
    Tenant b = createTenant("company");
    EXPECT_EQ(h.get("/api/v1/pos/sessions/" + sid, b.token).status, 404);
    EXPECT_EQ(h.get("/api/v1/pos/sessions/" + sid + "/report.pdf", b.token).status, 404);
    EXPECT_EQ(post("/api/v1/pos/sales", {{"session_id", sid}, {"lines", {{{"product_id", s.p1}, {"quantity", "1"}}}},
                                        {"payments", {{{"method", "cash"}, {"amount", "1500.00"}}}}},
                   b.token).status, 404);
}

TEST(Pos, DiscountAboveTheLimitNeedsASupervisorApprovalUsedOnce) {
    Shop s = shop();
    std::string sid = openSession(s, s.cashier);
    json body = {{"session_id", sid}, {"lines", {{{"product_id", s.p1}, {"quantity", "1"}, {"discount_bp", 2000}}}},
                 {"payments", {{{"method", "cash"}, {"amount", "1200.00"}}}}};
    auto r = post("/api/v1/pos/sales", body, s.cashier);
    EXPECT_EQ(r.status, 403);
    EXPECT_EQ(r.body["error"]["code"], "approval_required");
    // Wrong password, self-approval, then a real approval.
    EXPECT_EQ(post("/api/v1/pos/approvals", {{"email", s.managerEmail}, {"password", "wrong"}, {"action", "discount"}}, s.cashier).status,
              401);
    EXPECT_EQ(post("/api/v1/pos/approvals", {{"email", s.cashierEmail}, {"password", "Membre-password-2026"}, {"action", "discount"}},
                   s.cashier).status, 401);  // a cashier cannot approve
    body["approval_token"] = approval(s, s.cashier, "discount", sid);
    auto ok = post("/api/v1/pos/sales", body, s.cashier);
    ASSERT_EQ(ok.status, 201) << ok.raw;
    EXPECT_EQ(ok.body["receipt"]["discount"], "300.00");
    auto again = post("/api/v1/pos/sales", body, s.cashier);
    EXPECT_EQ(again.body["error"]["code"], "approval_invalid");
    // Within the limit (10 %), no approval.
    json small = {{"session_id", sid}, {"lines", {{{"product_id", s.p1}, {"quantity", "1"}, {"discount_bp", 1000}}}},
                  {"payments", {{{"method", "cash"}, {"amount", "1350.00"}}}}};
    EXPECT_EQ(post("/api/v1/pos/sales", small, s.cashier).status, 201);
}

TEST(Pos, ReturnsAndVoidsUseCreditNotesRefundsAndRestock) {
    auto& h = Harness::get();
    Shop s = shop();
    std::string sid = openSession(s, s.cashier);
    auto sale = post("/api/v1/pos/sales", {{"session_id", sid}, {"lines", {{{"product_id", s.p1}, {"quantity", "3"}}}},
                                          {"payments", {{{"method", "cash"}, {"amount", "4500.00"}}}}},
                     s.cashier);
    ASSERT_EQ(sale.status, 201);
    std::string rid = sale.body["receipt"]["id"];
    EXPECT_EQ(stockOf(s.t, s.p1), "17000");
    json ret = {{"session_id", sid}, {"reason", "Article défectueux"}, {"lines", {{{"line_no", 1}, {"quantity", "1"}}}},
                {"refunds", {{{"method", "cash"}, {"amount", "1500.00"}}}}};
    EXPECT_EQ(post("/api/v1/pos/sales/" + rid + "/return", ret, s.cashier).body["error"]["code"], "approval_required");
    ret["approval_token"] = approval(s, s.cashier, "return", sid);
    auto r = post("/api/v1/pos/sales/" + rid + "/return", ret, s.cashier);
    ASSERT_EQ(r.status, 201) << r.raw;
    EXPECT_EQ(r.body["credit_note"]["total"], "1500.00");
    EXPECT_EQ(r.body["receipt"]["credited"], "1500.00");
    EXPECT_EQ(stockOf(s.t, s.p1), "18000");
    // Without explicit refunds, the server refunds the credit note total in cash.
    auto r2 = post("/api/v1/pos/sales/" + rid + "/return",
                   {{"session_id", sid}, {"reason", "Retour client"}, {"lines", {{{"line_no", 1}, {"quantity", "1"}}}}}, s.manager);
    ASSERT_EQ(r2.status, 201) << r2.raw;
    EXPECT_EQ(r2.body["credit_note"]["total"], "1500.00");
    EXPECT_EQ(r2.body["receipt"]["credited"], "3000.00");
    EXPECT_EQ(h.su("SELECT count(*) FROM payments WHERE document_id = '" + r2.body["credit_note"]["id"].get<std::string>() +
                   "' AND direction = 'out' AND note = 'pos:cash' AND amount_minor = 150000")[0][0].as<int>(), 1);
    // Cannot return more than what is left; partially returned receipts cannot be voided.
    json tooMuch = {{"session_id", sid}, {"reason", "Retour"}, {"lines", {{{"line_no", 1}, {"quantity", "3"}}}},
                    {"refunds", {{{"method", "cash"}, {"amount", "4500.00"}}}}};
    EXPECT_EQ(post("/api/v1/pos/sales/" + rid + "/return", tooMuch, s.manager).status, 422);
    EXPECT_EQ(post("/api/v1/pos/sales/" + rid + "/void", {{"session_id", sid}, {"reason", "Erreur"}}, s.manager).body["error"]["code"],
              "already_partially_returned");
    // Void of a fresh receipt by the cash manager (own permission): full refund by the original method.
    std::string msid = sid;
    auto sale2 = post("/api/v1/pos/sales", {{"session_id", sid}, {"lines", {{{"product_id", s.p2}, {"quantity", "2"}}}},
                                           {"payments", {{{"method", "cash"}, {"amount", "5000.00"}}}}},
                      s.cashier);
    std::string rid2 = sale2.body["receipt"]["id"];
    auto v = post("/api/v1/pos/sales/" + rid2 + "/void", {{"session_id", msid}, {"reason", "Erreur de saisie"}}, s.manager);
    ASSERT_EQ(v.status, 201) << v.raw;
    EXPECT_EQ(v.body["receipt"]["status"], "credited");
    EXPECT_EQ(stockOf(s.t, s.p2), "20000");
    int audited = h.su("SELECT count(*) FROM audit_log WHERE establishment_id = '" + s.t.id + "' AND action IN ('pos.return','pos.void')")[0][0]
                      .as<int>();
    EXPECT_EQ(audited, 3);
}

TEST(Pos, CashMovementsClosingVarianceAndZReport) {
    auto& h = Harness::get();
    Shop s = shop();
    std::string sid = openSession(s, s.cashier, "10000.00");
    ASSERT_EQ(post("/api/v1/pos/sales", {{"session_id", sid}, {"lines", {{{"product_id", s.p2}, {"quantity", "2"}}}},
                                        {"payments", {{{"method", "cash"}, {"amount", "5000.00"}}}}},
                   s.cashier).status, 201);
    ASSERT_EQ(post("/api/v1/pos/sessions/" + sid + "/cash-movements", {{"kind", "cash_in"}, {"amount", "1000.00"}, {"reason", "Monnaie"}},
                   s.cashier).status, 201);
    json out = {{"kind", "cash_out"}, {"amount", "3000.00"}, {"reason", "Dépôt banque"}};
    EXPECT_EQ(post("/api/v1/pos/sessions/" + sid + "/cash-movements", out, s.cashier).body["error"]["code"], "approval_required");
    out["approval_token"] = approval(s, s.cashier, "cash_out", sid);
    ASSERT_EQ(post("/api/v1/pos/sessions/" + sid + "/cash-movements", out, s.cashier).status, 201);
    // Expected cash = 10000 + 5000 + 1000 - 3000 = 13000; the cashier counts 12900.
    auto c = post("/api/v1/pos/sessions/" + sid + "/close", {{"counted", {{"cash", "12900.00"}}}, {"note", "Billet abîmé"}}, s.cashier);
    ASSERT_EQ(c.status, 200) << c.raw;
    EXPECT_EQ(c.body["status"], "closed");
    EXPECT_FALSE(c.body.contains("cash_variance"));  // blind closing for the cashier
    auto m = h.get("/api/v1/pos/sessions/" + sid, s.manager);
    EXPECT_EQ(m.body["cash_expected"], "13000.00");
    EXPECT_EQ(m.body["cash_variance"], "-100.00");
    // Closed session: no more sales, Z report available, immutable.
    EXPECT_EQ(post("/api/v1/pos/sales", {{"session_id", sid}, {"lines", {{{"product_id", s.p1}, {"quantity", "1"}}}},
                                        {"payments", {{{"method", "cash"}, {"amount", "1500.00"}}}}},
                   s.cashier).body["error"]["code"], "session_closed");
    auto z = h.get("/api/v1/pos/sessions/" + sid + "/report.pdf", s.manager);
    EXPECT_EQ(z.status, 200);
    EXPECT_EQ(z.raw.rfind("%PDF", 0), 0u);
    EXPECT_THROW(h.su("UPDATE pos_sessions SET cash_counted_minor = 0 WHERE id = '" + sid + "'"), pqxx::sql_error);
    // A new session gets the next Z number.
    auto next = post("/api/v1/pos/sessions", {{"register_id", s.reg}}, s.cashier);
    EXPECT_EQ(next.body["number"], 2);
}

TEST(Pos, EstablishmentPromoCodes) {
    Shop s = shop();
    std::string sid = openSession(s, s.cashier);
    ASSERT_EQ(post("/api/v1/pos/promo-codes", {{"code", "fete"}, {"percent_bp", 1000}, {"max_redemptions", 1}}, s.t.token).status, 201);
    json body = {{"session_id", sid}, {"promo_code", "FETE"}, {"lines", {{{"product_id", s.p2}, {"quantity", "2"}}}},
                 {"payments", {{{"method", "cash"}, {"amount", "4500.00"}}}}};
    auto r = post("/api/v1/pos/sales", body, s.cashier);
    ASSERT_EQ(r.status, 201) << r.raw;
    EXPECT_EQ(r.body["receipt"]["total"], "4500.00");
    EXPECT_EQ(post("/api/v1/pos/sales", body, s.cashier).body["error"]["code"], "promo_exhausted");
    // A cashier cannot create promo codes.
    EXPECT_EQ(post("/api/v1/pos/promo-codes", {{"code", "CADEAU"}, {"percent_bp", 5000}}, s.cashier).status, 403);
}

// Nhill Business: the accountant reviews every cash session and its Z report without
// being able to sell or to configure the point of sale.
TEST(Pos, AccountantReadsSessionsButCannotSell) {
    auto& h = Harness::get();
    Shop s = shop();
    std::string sid = openSession(s, s.cashier);
    std::string acc = addMember(s.t, "accountant");
    auto list = h.get("/api/v1/pos/sessions", acc);
    ASSERT_EQ(list.status, 200) << list.raw;
    ASSERT_EQ(list.body["items"].size(), 1u);
    EXPECT_FALSE(list.body["items"][0].contains("cash_expected"));  // blind view: no supervision right
    EXPECT_EQ(h.get("/api/v1/pos/sessions/" + sid, acc).status, 200);
    EXPECT_EQ(h.get("/api/v1/pos/sessions/" + sid + "/report.pdf", acc).status, 200);
    EXPECT_EQ(h.get("/api/v1/pos/registers", acc).status, 200);
    EXPECT_EQ(post("/api/v1/pos/sales", {{"session_id", sid}, {"lines", {{{"product_id", s.p1}, {"quantity", "1"}}}},
                                         {"payments", {{{"method", "cash"}, {"amount", "1500.00"}}}}},
                   acc).status, 403);
    EXPECT_EQ(post("/api/v1/pos/sessions", {{"register_id", s.reg}}, acc).status, 403);
    EXPECT_EQ(h.request("PATCH", "/api/v1/pos/settings", {{"max_discount_bp", 5000}}, acc, newKey()).status, 403);
    // A sales clerk without POS reporting only sees their own sessions (none here).
    std::string clerk = addMember(s.t, "sales");
    auto own = h.get("/api/v1/pos/sessions", clerk);
    ASSERT_EQ(own.status, 200);
    EXPECT_EQ(own.body["items"].size(), 0u);
    EXPECT_EQ(h.get("/api/v1/pos/sessions/" + sid, clerk).status, 404);
}

// Cross-tenant checks on the POS, support and billing routes: knowing another
// establishment's identifiers (session, receipt, register, ticket, order) gives no access.
TEST(Isolation, PosSupportAndBillingIdentifiersOfAnotherEstablishmentAreUseless) {
    auto& h = Harness::get();
    Shop a = shop();
    std::string sid = openSession(a, a.t.token);
    auto sale = post("/api/v1/pos/sales", {{"session_id", sid}, {"lines", {{{"product_id", a.p1}, {"quantity", "1"}}}},
                                          {"payments", {{{"method", "cash"}, {"amount", "1500.00"}}}}},
                     a.t.token);
    ASSERT_EQ(sale.status, 201) << sale.raw;
    std::string rid = sale.body["receipt"]["id"];
    auto ticket = post("/api/v1/support/tickets", {{"category", "question"}, {"subject", "Question A"}, {"body", "Bonjour"}}, a.t.token);
    ASSERT_EQ(ticket.status, 201) << ticket.raw;
    std::string tid = ticket.body["id"];

    Shop b = shop();  // a second establishment with its own register and owner
    std::string bsid = openSession(b, b.t.token);
    const std::string& tok = b.t.token;
    EXPECT_EQ(h.get("/api/v1/pos/sessions/" + sid, tok).status, 404);
    EXPECT_EQ(h.get("/api/v1/pos/sessions/" + sid + "/report.pdf", tok).status, 404);
    EXPECT_EQ(h.get("/api/v1/pos/sales/" + rid + "/receipt.pdf", tok).status, 404);
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + rid, tok).status, 404);
    EXPECT_EQ(post("/api/v1/pos/sales/" + rid + "/return",
                   {{"session_id", bsid}, {"reason", "Retour"}, {"lines", {{{"line_no", 1}, {"quantity", "1"}}}}}, tok).status, 404);
    EXPECT_EQ(post("/api/v1/pos/sales/" + rid + "/void", {{"session_id", bsid}, {"reason", "Erreur"}}, tok).status, 404);
    EXPECT_EQ(post("/api/v1/pos/sessions/" + sid + "/cash-movements",
                   {{"kind", "cash_in"}, {"amount", "10.00"}, {"reason", "Monnaie"}}, tok).status, 404);
    EXPECT_EQ(post("/api/v1/pos/sessions/" + sid + "/close", {{"counted", {{"cash", "0.00"}}}}, tok).status, 404);
    EXPECT_EQ(post("/api/v1/pos/sales", {{"session_id", sid}, {"lines", {{{"product_id", b.p1}, {"quantity", "1"}}}},
                                         {"payments", {{{"method", "cash"}, {"amount", "1500.00"}}}}}, tok).status, 404);
    EXPECT_EQ(h.request("PATCH", "/api/v1/pos/registers/" + a.reg, {{"name", "Pirate"}}, tok, newKey()).status, 404);
    EXPECT_EQ(h.request("PUT", "/api/v1/pos/registers/" + a.reg + "/users", {{"user_ids", json::array()}}, tok, newKey()).status, 404);
    EXPECT_EQ(h.get("/api/v1/pos/catalog?register_id=" + a.reg, tok).status, 404);
    EXPECT_EQ(h.get("/api/v1/support/tickets/" + tid, tok).status, 404);
    EXPECT_EQ(post("/api/v1/support/tickets/" + tid + "/messages", {{"body", "Intrusion"}}, tok).status, 404);
    // Lists only show the caller's own rows.
    auto sessions = h.get("/api/v1/pos/sessions", tok);
    ASSERT_EQ(sessions.status, 200);
    for (const auto& s : sessions.body["items"]) EXPECT_NE(s["id"], sid);
    auto tickets = h.get("/api/v1/support/tickets", tok);
    ASSERT_EQ(tickets.status, 200);
    for (const auto& t : tickets.body["items"]) EXPECT_NE(t["id"], tid);
    // Nothing was written into establishment A by B.
    EXPECT_EQ(h.su("SELECT count(*) FROM support_ticket_messages WHERE ticket_id = '" + tid + "'")[0][0].as<int>(), 1);
    EXPECT_EQ(h.su("SELECT name FROM pos_registers WHERE id = '" + a.reg + "'")[0][0].as<std::string>(), "Caisse 1");
}
