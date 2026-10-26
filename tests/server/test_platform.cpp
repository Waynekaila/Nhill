// Platform scenarios: migrations, establishment creation, duplicates, trial,
// authentication, establishment codes, join flow, administration console.
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>

#include "nhill/establishment_code.hpp"
#include "harness.hpp"
#include "migrate.hpp"
#include "util/crypto.hpp"

using namespace nht;
namespace fs = std::filesystem;

TEST(Migrations, ApplicationRoleCanCheckPendingMigrationsAfterFirstRun) {
    // Must hold right after the very first migration run (regression: a
    // migration revoking table rights used to remove this grant).
    auto& h = Harness::get();
    EXPECT_NO_THROW(h.asApp(std::nullopt, "SELECT count(*) FROM schema_migrations"));
}

TEST(Migrations, AppliedOnceAndIdempotent) {
    auto& h = Harness::get();
    auto r = h.su("SELECT count(*) FROM schema_migrations");
    EXPECT_GE(r[0][0].as<int>(), 5);
    auto again = nh::runMigrations(h.ownerUrl, h.migrationsDir);
    EXPECT_TRUE(again.applied.empty());
    EXPECT_EQ(again.alreadyApplied.size(), static_cast<size_t>(r[0][0].as<int>()));
    EXPECT_TRUE(nh::pendingMigrations(h.appUrl, h.migrationsDir).empty());
}

TEST(Migrations, ModifiedMigrationIsRefused) {
    auto& h = Harness::get();
    fs::path tmp = fs::temp_directory_path() / uniq("migr");
    fs::create_directories(tmp);
    for (const auto& e : fs::directory_iterator(h.migrationsDir)) fs::copy(e.path(), tmp / e.path().filename());
    std::ofstream(tmp / "0001_platform.sql", std::ios::app) << "\n-- tampered\n";
    EXPECT_THROW(nh::runMigrations(h.ownerUrl, tmp.string()), std::runtime_error);
    fs::remove_all(tmp);
}

TEST(Migrations, ApplicationRoleHasNoDdlAndNoBypass) {
    auto& h = Harness::get();
    EXPECT_THROW(h.asApp(std::nullopt, "CREATE TABLE hack (id int)"), pqxx::sql_error);
    EXPECT_THROW(h.asApp(std::nullopt, "DELETE FROM audit_log"), pqxx::sql_error);
    EXPECT_THROW(h.asApp(std::nullopt, "UPDATE audit_log SET action = 'x'"), pqxx::sql_error);
    auto r = h.su("SELECT rolbypassrls, rolsuper FROM pg_roles WHERE rolname = 'nhill_app'");
    EXPECT_FALSE(r[0][0].as<bool>());
    EXPECT_FALSE(r[0][1].as<bool>());
}

TEST(Establishment, CreateCompanyWithTrialAndCode) {
    auto& h = Harness::get();
    Tenant t = createTenant("company", {{"rccm", "CD/KNG/RCCM/" + uniq("26-B-")}, {"nif", uniq("A")}});
    EXPECT_TRUE(nhill::code::normalize(t.code).has_value());
    auto me = h.get("/api/v1/me", t.token);
    ASSERT_EQ(me.status, 200) << me.raw;
    EXPECT_EQ(me.body["role"], "owner");
    EXPECT_EQ(me.body["subscription"]["status"], "trialing");
    EXPECT_EQ(me.body["subscription"]["is_trial"], true);
    EXPECT_GE(me.body["subscription"]["days_left"].get<int>(), 29);
    // Defaults created from the CD profile
    auto taxes = h.get("/api/v1/tax-rates", t.token);
    bool hasVat = false;
    for (auto& x : taxes.body["items"]) hasVat |= x["code"] == "TVA16" && x["rate_bp"] == 1600;
    EXPECT_TRUE(hasVat);
    // History recorded
    auto audit = h.su("SELECT count(*) FROM audit_log WHERE establishment_id = '" + t.id + "' AND action = 'establishment.created'");
    EXPECT_EQ(audit[0][0].as<int>(), 1);
    // The code is stored hashed and encrypted, never in clear text
    std::string canonical = *nhill::code::normalize(t.code);
    auto clear = h.su("SELECT count(*) FROM establishment_codes WHERE encode(code_cipher,'escape') LIKE '%" + canonical + "%'");
    EXPECT_EQ(clear[0][0].as<int>(), 0);
}

TEST(Establishment, IndividualEntrepreneurNeedsNoLegalIdAndHasOneSeat) {
    auto& h = Harness::get();
    Tenant t = createTenant("individual");
    auto me = h.get("/api/v1/me", t.token);
    EXPECT_EQ(me.body["establishment"]["kind"], "individual");
    auto members = h.get("/api/v1/members", t.token);
    EXPECT_EQ(members.body["seat_limit"], 1);
    EXPECT_EQ(members.body["members"].size(), 1u);
    auto inv = h.post("/api/v1/invitations", {{"email", uniq("x") + "@example.cd"}, {"role", "sales"}}, t.token);
    EXPECT_EQ(inv.status, 409);
    EXPECT_EQ(inv.body["error"]["code"], "user_limit_reached");
}

TEST(Establishment, ValidationRejectsBadInput) {
    auto& h = Harness::get();
    json body = {{"kind", "company"}, {"trade_name", "X"}, {"country_code", "CD"}, {"main_currency", "CDF"},
                 {"owner", {{"full_name", "A"}, {"email", "pas-un-email"}, {"password", "Kinshasa-2026-tests"}}}};
    auto r = h.post("/api/v1/establishments", body);
    EXPECT_EQ(r.status, 422);
    EXPECT_EQ(r.body["error"]["details"]["field"], "email");
    body["owner"]["email"] = uniq("v") + "@example.cd";
    body["owner"]["password"] = "court";
    r = h.post("/api/v1/establishments", body);
    EXPECT_EQ(r.body["error"]["details"]["reason"], "password_too_short");
    body["owner"]["password"] = "Kinshasa-2026-tests";
    body["legal_ids"] = {{"siret", "123"}};
    r = h.post("/api/v1/establishments", body);
    EXPECT_EQ(r.body["error"]["details"]["reason"], "unknown_identifier");
    // VAT-registered establishments in CD need a NIF (profile rule)
    body["legal_ids"] = json::object();
    body["vat_registered"] = true;
    r = h.post("/api/v1/establishments", body);
    EXPECT_EQ(r.body["error"]["details"]["field"], "legal_ids.nif");
}

TEST(Establishment, DuplicateLegalIdIsRefusedWithoutRevealingName) {
    auto& h = Harness::get();
    std::string rccm = "CD/KNG/RCCM/" + uniq("DUP-");
    createTenant("company", {{"rccm", rccm}});
    try {
        createTenant("company", {{"rccm", "  " + rccm + " "}});  // same after normalisation
        FAIL() << "duplicate accepted";
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("409"), std::string::npos);
        EXPECT_NE(msg.find("establishment_exists"), std::string::npos);
        EXPECT_EQ(msg.find("Boutique"), std::string::npos);  // no name leaked
    }
    // A similar *name* never triggers an automatic attachment: the attachment
    // request gets the same answer whether the identifier exists or not.
    auto a = h.post("/api/v1/establishments/attachment-requests",
                    {{"country_code", "CD"}, {"identifier_kind", "rccm"}, {"identifier_value", rccm},
                     {"full_name", "Requérant"}, {"email", "req@example.cd"}});
    auto b = h.post("/api/v1/establishments/attachment-requests",
                    {{"country_code", "CD"}, {"identifier_kind", "rccm"}, {"identifier_value", "INCONNU-" + uniq()},
                     {"full_name", "Requérant"}, {"email", "req@example.cd"}});
    EXPECT_EQ(a.status, 202);
    EXPECT_EQ(b.status, 202);
    EXPECT_EQ(a.raw, b.raw);
}

TEST(Establishment, ConcurrentCreationWithSameIdentifier) {
    std::string nif = uniq("NIF-RACE-");
    auto attempt = [&]() {
        try {
            createTenant("company", {{"nif", nif}});
            return 201;
        } catch (const std::runtime_error& e) {
            return std::string(e.what()).find("409") != std::string::npos ? 409 : 500;
        }
    };
    std::vector<std::future<int>> fs;
    for (int i = 0; i < 6; ++i) fs.push_back(std::async(std::launch::async, attempt));
    int created = 0, conflicts = 0;
    for (auto& f : fs) {
        int s = f.get();
        created += s == 201;
        conflicts += s == 409;
    }
    EXPECT_EQ(created, 1);
    EXPECT_EQ(conflicts, 5);
}

TEST(Trial, CannotBeResetByRecreatingTheAccount) {
    auto& h = Harness::get();
    Tenant first = createTenant("individual");
    // Same owner (existing account, proven by its password) creates a second business.
    Tenant second = createTenant("individual", json::object(), false, first.ownerEmail);
    auto me = h.get("/api/v1/me", second.token);
    EXPECT_EQ(me.body["subscription"]["status"], "suspended");
    EXPECT_EQ(me.body["subscription"]["is_trial"], false);
    // Read allowed, write refused (subscription is the source of truth)
    EXPECT_EQ(h.get("/api/v1/products", second.token).status, 200);
    auto w = h.post("/api/v1/products", {{"sku", "S1"}, {"name", "P"}, {"sale_price", "1.00"}}, second.token);
    EXPECT_EQ(w.status, 402);
    EXPECT_EQ(w.body["error"]["code"], "subscription_suspended");
    // Same legal identifier with a different owner: blocked by duplicate check anyway.
}

TEST(Auth, LoginRefreshRotationAndReuseDetection) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    auto bad = h.post("/api/v1/auth/login", {{"email", t.ownerEmail}, {"password", "mauvais-mot-de-passe"}});
    EXPECT_EQ(bad.status, 401);
    auto unknown = h.post("/api/v1/auth/login", {{"email", "personne-" + uniq() + "@example.cd"}, {"password", "x-x-x-x-x-x"}});
    EXPECT_EQ(unknown.status, 401);
    EXPECT_EQ(bad.body["error"]["code"], unknown.body["error"]["code"]);  // no account enumeration
    auto ok = h.post("/api/v1/auth/login", {{"email", t.ownerEmail}, {"password", t.password}});
    ASSERT_EQ(ok.status, 200) << ok.raw;
    std::string r1 = ok.body["refresh_token"];
    auto ref = h.post("/api/v1/auth/refresh", {{"refresh_token", r1}});
    ASSERT_EQ(ref.status, 200);
    std::string access2 = ref.body["access_token"];
    EXPECT_EQ(h.get("/api/v1/me", access2).status, 200);
    // Old access token no longer valid after rotation
    EXPECT_EQ(h.get("/api/v1/me", ok.body["access_token"]).status, 401);
    // Replaying the old refresh token = theft signal: the session is revoked
    auto replay = h.post("/api/v1/auth/refresh", {{"refresh_token", r1}});
    EXPECT_EQ(replay.status, 401);
    EXPECT_EQ(h.get("/api/v1/me", access2).status, 401);
    auto audit = h.su("SELECT count(*) FROM audit_log WHERE action = 'auth.refresh_reuse_detected' AND establishment_id = '" + t.id + "'");
    EXPECT_EQ(audit[0][0].as<int>(), 1);
}

TEST(Auth, BruteForceIsThrottled) {
    auto& h = Harness::get();
    Tenant t = createTenant("individual");
    int last = 0;
    for (int i = 0; i < 10; ++i) last = h.post("/api/v1/auth/login", {{"email", t.ownerEmail}, {"password", "wrong-" + std::to_string(i)}}).status;
    EXPECT_EQ(last, 429);
    // Even the right password is refused while throttled
    EXPECT_EQ(h.post("/api/v1/auth/login", {{"email", t.ownerEmail}, {"password", t.password}}).status, 429);
}

TEST(Auth, LogoutAndPasswordChangeRevokeSessions) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    auto second = h.post("/api/v1/auth/login", {{"email", t.ownerEmail}, {"password", t.password}});
    std::string other = second.body["access_token"];
    auto ch = h.post("/api/v1/me/password", {{"current_password", t.password}, {"new_password", "Nouveau-mot-2026!"}}, t.token);
    EXPECT_EQ(ch.status, 204) << ch.raw;
    EXPECT_EQ(h.get("/api/v1/me", other).status, 401);    // other sessions closed
    EXPECT_EQ(h.get("/api/v1/me", t.token).status, 200);  // current one kept
    EXPECT_EQ(h.post("/api/v1/auth/logout", json::object(), t.token).status, 204);
    EXPECT_EQ(h.get("/api/v1/me", t.token).status, 401);
}

TEST(Codes, ValidationIsGenericForEveryFailure) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    auto ok = h.post("/api/v1/join/check-code", {{"code", t.code}});
    ASSERT_EQ(ok.status, 200) << ok.raw;
    EXPECT_FALSE(ok.body["join_ticket"].get<std::string>().empty());

    auto malformed = h.post("/api/v1/join/check-code", {{"code", "NH-0000"}});
    auto unknown = h.post("/api/v1/join/check-code", {{"code", "NH-AAAA-BBBB-CCCC"}});
    EXPECT_EQ(malformed.status, 422);
    EXPECT_EQ(unknown.status, 422);
    EXPECT_EQ(malformed.body["error"]["code"], "code_unusable");
    EXPECT_EQ(unknown.body["error"]["code"], "code_unusable");

    Tenant expired = createTenant("company");
    h.su("UPDATE establishment_codes SET created_at = now() - interval '40 days', expires_at = now() - interval '1 minute' "
         "WHERE establishment_id = '" + expired.id + "'");
    EXPECT_EQ(h.post("/api/v1/join/check-code", {{"code", expired.code}}).body["error"]["code"], "code_unusable");

    Tenant suspended = createTenant("company");
    h.su("UPDATE establishments SET status = 'suspended' WHERE id = '" + suspended.id + "'");
    EXPECT_EQ(h.post("/api/v1/join/check-code", {{"code", suspended.code}}).body["error"]["code"], "code_unusable");

    Tenant revoked = createTenant("company");
    auto rot = h.post("/api/v1/establishment/code/rotate", {{"reason", "fuite"}}, revoked.token);
    ASSERT_EQ(rot.status, 200);
    EXPECT_NE(rot.body["code"], revoked.code);
    EXPECT_EQ(h.post("/api/v1/join/check-code", {{"code", revoked.code}}).body["error"]["code"], "code_unusable");
    EXPECT_EQ(h.post("/api/v1/join/check-code", {{"code", rot.body["code"]}}).status, 200);
}

TEST(Codes, CodeAloneGivesNoAccess) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    auto chk = h.post("/api/v1/join/check-code", {{"code", t.code}});
    auto noPass = h.post("/api/v1/join/login", {{"join_ticket", chk.body["join_ticket"]}, {"email", t.ownerEmail}, {"password", "faux-faux-faux"}});
    EXPECT_EQ(noPass.status, 401);
    auto stranger = h.post("/api/v1/join/activate", {{"join_ticket", chk.body["join_ticket"]}, {"email", "intrus@example.cd"},
                                                     {"activation_code", "1234-5678"}, {"full_name", "Intrus"},
                                                     {"password", "Intrus-password-2026"}});
    EXPECT_EQ(stranger.status, 401);
    auto good = h.post("/api/v1/join/login", {{"join_ticket", chk.body["join_ticket"]}, {"email", t.ownerEmail}, {"password", t.password}});
    EXPECT_EQ(good.status, 200);
}

TEST(Codes, ValidationAttemptsAreLimited) {
    auto& h = Harness::get();
    // Every request in this binary comes from 127.0.0.1: use a fresh window by clearing the bucket.
    h.su("DELETE FROM auth_throttle WHERE bucket LIKE 'code:ip:%'");
    int last = 0;
    for (int i = 0; i < 12; ++i) last = h.post("/api/v1/join/check-code", {{"code", "NH-AAAA-BBBB-CCC" + std::to_string(i % 10)}}).status;
    EXPECT_EQ(last, 429);
    h.su("DELETE FROM auth_throttle WHERE bucket LIKE 'code:ip:%'");
}

TEST(Codes, JoinWithInvitationGivesTheInvitedRole) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string token = addMember(t, "sales");
    auto me = h.get("/api/v1/me", token);
    EXPECT_EQ(me.body["role"], "sales");
    EXPECT_EQ(me.body["establishment"]["id"], t.id);
    // An activation code cannot be reused
    auto members = h.get("/api/v1/members", t.token);
    EXPECT_EQ(members.body["members"].size(), 2u);
    EXPECT_EQ(members.body["invitations"].size(), 0u);
}

TEST(Codes, CollisionIsRetriedWithANewCode) {
    auto& h = Harness::get();
    Tenant first = createTenant("company");
    std::string taken = *nhill::code::normalize(first.code);
    int calls = 0;
    nh::codes::setGeneratorForTests([&]() -> std::string {
        if (calls++ == 0) return taken;  // forced collision on the first try
        return nhill::code::generate([](uint8_t* o, size_t n) { nh::crypto::randomFill(o, n); });
    });
    Tenant second = createTenant("company");
    nh::codes::setGeneratorForTests(nullptr);
    EXPECT_EQ(calls, 2);
    EXPECT_NE(*nhill::code::normalize(second.code), taken);
    (void)h;
}

TEST(Codes, ScheduledRotationIsAtomicAndConcurrencySafe) {
    auto& h = Harness::get();
    std::vector<Tenant> ts;
    for (int i = 0; i < 6; ++i) ts.push_back(createTenant("company"));
    std::string ids;
    for (auto& t : ts) ids += (ids.empty() ? "'" : ",'") + t.id + "'";
    h.su("UPDATE establishment_codes SET created_at = now() - interval '31 days', expires_at = now() - interval '1 second' "
         "WHERE status = 'active' AND establishment_id IN (" + ids + ")");
    // Two workers race on the same expired codes.
    auto f1 = std::async(std::launch::async, [&] { return nh::jobs::rotateCodes(h.env()).codesRotated; });
    auto f2 = std::async(std::launch::async, [&] { return nh::jobs::rotateCodes(h.env()).codesRotated; });
    int total = f1.get() + f2.get();
    EXPECT_GE(total, 6);
    for (auto& t : ts) {
        auto r = h.su("SELECT count(*) FILTER (WHERE status = 'active'), count(*) FILTER (WHERE status = 'superseded') "
                      "FROM establishment_codes WHERE establishment_id = '" + t.id + "'");
        EXPECT_EQ(r[0][0].as<int>(), 1) << "exactly one active code";
        EXPECT_EQ(r[0][1].as<int>(), 1) << "exactly one rotation";
        // The owner, connected before the rotation, stays connected.
        EXPECT_EQ(h.get("/api/v1/me", t.token).status, 200);
        // The new code is valid for 30 days, the old one is refused.
        auto cur = h.get("/api/v1/establishment/code", t.token);
        EXPECT_NE(cur.body["code"], t.code);
        EXPECT_EQ(h.post("/api/v1/join/check-code", {{"code", t.code}}).status, 422);
        auto notif = h.su("SELECT count(*) FROM notifications WHERE establishment_id = '" + t.id + "' AND kind = 'code_rotated'");
        EXPECT_EQ(notif[0][0].as<int>(), 1);
    }
    h.su("DELETE FROM auth_throttle WHERE bucket LIKE 'code:ip:%'");
}

TEST(Codes, OnlyAuthorisedRolesSeeTheCode) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string viewer = addMember(t, "viewer");
    EXPECT_EQ(h.get("/api/v1/establishment/code", viewer).status, 403);
    EXPECT_EQ(h.get("/api/v1/establishment/code", t.token).status, 200);
}


TEST(Admin, TotpVectorsRfc6238) {
    // RFC 6238 appendix B, SHA-1, secret "12345678901234567890", 8 digits -> we use the last 6.
    std::string secret = "12345678901234567890";
    EXPECT_EQ(nh::crypto::totpCode(secret, 59 / 30), "287082");
    EXPECT_EQ(nh::crypto::totpCode(secret, 1111111109 / 30), "081804");
    EXPECT_EQ(nh::crypto::totpCode(secret, 1234567890 / 30), "005924");
}

TEST(Admin, ConsoleRequiresPasswordAndTotpAndCannotReadBusinessData) {
    auto& h = Harness::get();
    std::string admin = adminLogin();
    Tenant t = createTenant("company");
    std::string customer = createCustomer(t, "Client confidentiel");
    auto list = h.get("/api/v1/admin/establishments?search=" + t.id, admin);
    ASSERT_EQ(list.status, 200) << list.raw;
    EXPECT_EQ(list.body["total"], 1);
    auto detail = h.get("/api/v1/admin/establishments/" + t.id, admin);
    EXPECT_EQ(detail.status, 200);
    EXPECT_EQ(detail.raw.find("Client confidentiel"), std::string::npos);
    // No admin route exposes customers/products; and a tenant token is not an admin token.
    EXPECT_EQ(h.get("/api/v1/admin/establishments", t.token).status, 401);
    EXPECT_EQ(h.get("/api/v1/customers", admin).status, 401);
    // Database level: without a tenant context, business rows are invisible.
    EXPECT_EQ(h.asApp(std::nullopt, "SELECT count(*) FROM parties")[0][0].as<int>(), 0);
    // Stats are aggregates only
    auto stats = h.get("/api/v1/admin/stats", admin);
    EXPECT_EQ(stats.status, 200);
    EXPECT_GE(stats.body["establishments_by_status"]["active"].get<int>(), 1);
}

TEST(Admin, WrongTotpIsRefusedAndRolesAreEnforced) {
    auto& h = Harness::get();
    std::string auditor = adminLogin("auditor");
    Tenant t = createTenant("company");
    auto s = h.post("/api/v1/admin/establishments/" + t.id + "/suspend", {{"reason", "Contrôle conformité"}}, auditor);
    EXPECT_EQ(s.status, 403);
    auto plan = h.post("/api/v1/admin/plans", {{"code", "x"}, {"name_fr", "x"}, {"name_en", "x"}, {"billing_period", "monthly"},
                                               {"price", "1.00"}, {"currency", "USD"}}, auditor);
    EXPECT_EQ(plan.status, 403);
    auto bad = h.post("/api/v1/admin/auth/login", {{"email", "nobody@nhill.test"}, {"password", "x"}, {"totp_code", "000000"}});
    EXPECT_EQ(bad.status, 401);
}

TEST(Admin, SuspensionRevokesSessionsAndIsAudited) {
    auto& h = Harness::get();
    std::string admin = adminLogin("support");
    Tenant t = createTenant("company");
    auto r = h.post("/api/v1/admin/establishments/" + t.id + "/suspend", {{"reason", "Fraude suspectée"}}, admin);
    EXPECT_EQ(r.status, 204) << r.raw;
    EXPECT_EQ(h.get("/api/v1/me", t.token).status, 401);
    auto login = h.post("/api/v1/auth/login", {{"email", t.ownerEmail}, {"password", t.password}});
    ASSERT_EQ(login.status, 200);
    // Read-only while suspended by the platform
    EXPECT_EQ(h.post("/api/v1/parties", {{"is_customer", true}, {"name", "x"}}, login.body["access_token"]).status, 403);
    auto reissue = h.post("/api/v1/admin/establishments/" + t.id + "/codes/reissue", {{"reason", "Code divulgué"}}, admin);
    EXPECT_EQ(reissue.status, 204);
    EXPECT_EQ(reissue.raw.find("NH-"), std::string::npos);  // the admin never sees the code
    auto a = h.su("SELECT count(*) FROM audit_log WHERE establishment_id = '" + t.id + "' AND action LIKE 'admin.%'");
    EXPECT_EQ(a[0][0].as<int>(), 2);
}

TEST(Migrations, EverythingLivesInTheNhillSchemaClosedToOtherRoles) {
    auto& h = Harness::get();
    // Simulates the Supabase API roles: they get nothing on the Nhill schema.
    h.su("DO $$ BEGIN IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'anon') THEN CREATE ROLE anon NOLOGIN; END IF; "
         "IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'authenticated') THEN CREATE ROLE authenticated NOLOGIN; END IF; END $$");
    EXPECT_EQ(h.su("SELECT count(*) FROM pg_tables WHERE schemaname = 'public'")[0][0].as<int>(), 0);
    EXPECT_GT(h.su("SELECT count(*) FROM pg_tables WHERE schemaname = 'nhill'")[0][0].as<int>(), 40);
    for (const char* role : {"anon", "authenticated"}) {
        std::string r = role;
        EXPECT_FALSE(h.su("SELECT has_schema_privilege('" + r + "', 'nhill', 'USAGE')")[0][0].as<bool>()) << r;
        EXPECT_FALSE(h.su("SELECT has_table_privilege('" + r + "', 'nhill.sales_documents', 'SELECT')")[0][0].as<bool>()) << r;
        EXPECT_FALSE(h.su("SELECT has_function_privilege('" + r + "', 'nhill.nh_platform_establishment_counts(uuid)', 'EXECUTE')")[0][0].as<bool>()) << r;
    }
    // Every business table has forced row-level security.
    EXPECT_EQ(h.su("SELECT count(*) FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace WHERE n.nspname = 'nhill' "
                   "AND c.relkind = 'r' AND c.relrowsecurity AND NOT c.relforcerowsecurity")[0][0].as<int>(), 0);
}
