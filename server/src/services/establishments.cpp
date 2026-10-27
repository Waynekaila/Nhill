// Establishment creation (option B), join flow (option A), profile, logo and code.
#include "nhill/establishment_code.hpp"
#include "nhill/money.hpp"
#include "nhill/validation.hpp"
#include "nhill/version.hpp"
#include "pdf/pdf.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"
#include "util/log.hpp"

namespace nh {

namespace v = nhill::validation;

// --------------------------------------------------------------------- images

ImageInfo validateImage(const std::string& b, const std::string& declaredMime) {
    constexpr size_t kMaxBytes = 2 * 1024 * 1024;
    if (b.empty()) throw invalid("logo", "empty");
    if (b.size() > kMaxBytes) throw invalid("logo", "too_large");
    auto u8 = [&](size_t i) { return static_cast<unsigned char>(b[i]); };
    ImageInfo info;
    if (b.size() > 24 && b.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0 && b.compare(12, 4, "IHDR") == 0) {
        info.mime = "image/png";
        info.width = static_cast<int>((u8(16) << 24) | (u8(17) << 16) | (u8(18) << 8) | u8(19));
        info.height = static_cast<int>((u8(20) << 24) | (u8(21) << 16) | (u8(22) << 8) | u8(23));
    } else if (b.size() > 4 && u8(0) == 0xFF && u8(1) == 0xD8) {
        info.mime = "image/jpeg";
        size_t i = 2;
        while (i + 9 < b.size()) {
            if (u8(i) != 0xFF) throw invalid("logo", "corrupted_image");
            unsigned char marker = u8(i + 1);
            if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
                i += 2;
                continue;
            }
            size_t len = (u8(i + 2) << 8) | u8(i + 3);
            bool sof = (marker >= 0xC0 && marker <= 0xCF) && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
            if (sof) {
                info.height = (u8(i + 5) << 8) | u8(i + 6);
                info.width = (u8(i + 7) << 8) | u8(i + 8);
                break;
            }
            if (len < 2) throw invalid("logo", "corrupted_image");
            i += 2 + len;
        }
    } else {
        throw invalid("logo", "unsupported_format");
    }
    if (!declaredMime.empty() && declaredMime.rfind(info.mime, 0) != 0) throw invalid("logo", "content_type_mismatch");
    if (info.width < 16 || info.height < 16 || info.width > 4096 || info.height > 4096)
        throw invalid("logo", "bad_dimensions");
    // Final authority: the PDF engine must be able to decode it.
    if (!pdf::probeImage(b, info.mime)) throw invalid("logo", "corrupted_image");
    return info;
}

namespace {

json legalIdsJson(db::Tx& tx, const std::string& est) {
    json ids = json::object();
    for (const auto& r : tx.q("SELECT kind, value FROM establishment_identifiers WHERE establishment_id = $1", est))
        ids[db::str(r[0])] = db::str(r[1]);
    return ids;
}

std::string fingerprint(Env& env, const std::string& material) {
    return crypto::hmacSha256Hex(env.cfg.fingerprintKey, material);
}

struct LegalId {
    std::string kind, value, normalized;
};

// Validates legal identifiers against the country profile.
std::vector<LegalId> parseLegalIds(const json& body, const json& profile, const std::string& kind, bool vatRegistered) {
    std::vector<LegalId> out;
    json given = body.contains("legal_ids") && body["legal_ids"].is_object() ? body["legal_ids"] : json::object();
    std::set<std::string> known;
    for (const auto& def : profile.at("legal_ids")) {
        std::string k = def.at("kind");
        known.insert(k);
        bool required = false;
        for (const auto& rk : def.value("required_for", json::array()))
            if (rk == kind) required = true;
        if (vatRegistered && def.value("required_if_vat_registered", false)) required = true;
        auto val = in::optStr(given, k, 64);
        if (!val) {
            if (required) throw invalid("legal_ids." + k, "required");
            continue;
        }
        auto norm = v::normalizeLegalId(*val);
        if (!norm || norm->size() < 3) throw invalid("legal_ids." + k, "invalid_identifier");
        out.push_back({k, *val, *norm});
    }
    for (auto it = given.begin(); it != given.end(); ++it)
        if (!known.count(it.key()) && !it.value().is_null()) throw invalid("legal_ids." + it.key(), "unknown_identifier");
    return out;
}

json establishmentJson(Env& env, db::Tx& tx, const std::string& est) {
    auto e = tx.one(
        "SELECT id, kind, trade_name, legal_name, country_code, city, address, sector, preferred_language, "
        "main_currency, email, phone, website, vat_registered, compliance_profile_code, logo_object_id IS NOT NULL, "
        "status, nh_iso(created_at), timezone FROM establishments WHERE id = $1",
        est);
    auto s = tx.one(
        "SELECT allow_negative_stock, tax_rounding, default_payment_terms_days, document_language, invoice_footer, "
        "payment_instructions FROM establishment_settings WHERE establishment_id = $1",
        est);
    const json& profile = env.profiles.get(db::str(e[14]));
    json notice = nullptr;
    if (db::boolean(e[13]) && profile.contains("document_notices") && profile["document_notices"].contains("vat_registered"))
        notice = profile["document_notices"]["vat_registered"];
    auto nullable = [](const pqxx::field& f) { return f.is_null() ? json(nullptr) : json(db::str(f)); };
    return {{"id", db::str(e[0])},
            {"kind", db::str(e[1])},
            {"trade_name", db::str(e[2])},
            {"legal_name", nullable(e[3])},
            {"country_code", db::str(e[4])},
            {"city", nullable(e[5])},
            {"address", nullable(e[6])},
            {"sector", nullable(e[7])},
            {"preferred_language", db::str(e[8])},
            {"main_currency", db::str(e[9])},
            {"email", nullable(e[10])},
            {"phone", nullable(e[11])},
            {"website", nullable(e[12])},
            {"vat_registered", db::boolean(e[13])},
            {"profile", db::str(e[14])},
            {"has_logo", db::boolean(e[15])},
            {"status", db::str(e[16])},
            {"created_at", db::str(e[17])},
            {"timezone", db::str(e[18])},
            {"legal_ids", legalIdsJson(tx, est)},
            {"settings",
             {{"allow_negative_stock", db::boolean(s[0])},
              {"tax_rounding", db::str(s[1])},
              {"default_payment_terms_days", s[2].as<int>()},
              {"document_language", db::str(s[3])},
              {"invoice_footer", db::str(s[4])},
              {"payment_instructions", db::str(s[5])}}},
            {"compliance_notice", notice},
            {"profile_validation_status", profile.value("validation_status", "")}};
}

void seedEstablishment(Env& /*env*/, db::Tx& tx, const std::string& est, const json& profile, bool vatRegistered,
                       const std::string& lang, const std::string& currency) {
    tx.q("INSERT INTO establishment_settings (establishment_id, tax_rounding, document_language) VALUES ($1, $2, $3)",
         est, profile.value("tax_rounding", "per_line") == "per_rate" ? "per_rate" : "per_line", lang);
    for (const auto& t : profile.at("tax_rates")) {
        std::string when = t.value("default_when", "");
        bool isDefault = (when == "vat_registered" && vatRegistered) || (when == "not_vat_registered" && !vatRegistered);
        tx.q("INSERT INTO tax_rates (establishment_id, code, label, rate_bp, kind, is_default) VALUES ($1,$2,$3,$4,$5,$6)",
             est, t.at("code").get<std::string>(), t.at(lang == "en" ? "label_en" : "label_fr").get<std::string>(),
             t.at("rate_bp").get<int>(), t.at("kind").get<std::string>(), isDefault);
    }
    tx.q("INSERT INTO warehouses (establishment_id, name, is_default) VALUES ($1, $2, true)", est,
         lang == "en" ? "Main warehouse" : "Dépôt principal");
    tx.q("INSERT INTO cash_accounts (establishment_id, name, kind, currency) VALUES ($1, $2, 'cash', $3)", est,
         lang == "en" ? "Cash" : "Caisse", currency);
    static const std::vector<std::pair<const char*, const char*>> categories = {
        {"Loyer", "Rent"},         {"Transport", "Transport"},       {"Salaires", "Salaries"},
        {"Fournitures", "Supplies"}, {"Énergie et eau", "Utilities"}, {"Communication", "Communication"},
        {"Taxes et redevances", "Taxes and fees"}, {"Autres", "Other"}};
    for (const auto& [fr, en] : categories)
        tx.q("INSERT INTO expense_categories (establishment_id, name) VALUES ($1, $2)", est, lang == "en" ? en : fr);
}

}  // namespace

void registerMetaRoutes(App& app, Env& env) {
    app.get("/health", {Access::Public}, [&env](Ctx& ctx) {
        bool dbOk = false;
        try {
            dbOk = db::run(env.pool, std::nullopt, [](db::Tx& tx) { return tx.one("SELECT 1")[0].as<int>() == 1; });
        } catch (...) {
        }
        ctx.json(dbOk ? 200 : 503, {{"status", dbOk ? "ok" : "degraded"}, {"database", dbOk}});
    });

    app.get("/api/v1/meta", {Access::Public}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json currencies = json::array();
            for (const auto& r : tx.q("SELECT code, minor_units, name_fr, name_en FROM currencies WHERE active ORDER BY code"))
                currencies.push_back({{"code", db::str(r[0])}, {"minor_units", r[1].as<int>()},
                                      {"name_fr", db::str(r[2])}, {"name_en", db::str(r[3])}});
            json ann = json::array();
            for (const auto& r : tx.q(
                     "SELECT kind, title_fr, title_en, body_fr, body_en, nh_iso(starts_at), nh_iso(ends_at), "
                     "min_client_version FROM announcements WHERE starts_at <= now() + interval '7 days' "
                     "AND (ends_at IS NULL OR ends_at > now()) ORDER BY starts_at"))
                ann.push_back({{"kind", db::str(r[0])}, {"title_fr", db::str(r[1])}, {"title_en", db::str(r[2])},
                               {"body_fr", db::str(r[3])}, {"body_en", db::str(r[4])}, {"starts_at", db::str(r[5])},
                               {"ends_at", r[6].is_null() ? json(nullptr) : json(db::str(r[6]))},
                               {"min_client_version", r[7].is_null() ? json(nullptr) : json(db::str(r[7]))}});
            json profiles = json::array();
            for (const auto& [code, p] : env.profiles.all())
                profiles.push_back({{"code", code}, {"country", p.value("country", json(nullptr))},
                                    {"name_fr", p.value("name_fr", "")}, {"name_en", p.value("name_en", "")}});
            return {{"product", nhill::kProductName},
                    {"version", nhill::kProductVersion},
                    {"api", nhill::kApiVersion},
                    {"min_client_version", nhill::kMinClientVersion},
                    {"currencies", currencies},
                    {"profiles", profiles},
                    {"announcements", ann},
                    {"trial_days", setting(tx, "trial_days", 30)}};
        });
        ctx.json(200, out);
    });

    // Form definition for a country: which legal identifiers exist / are required.
    app.get("/api/v1/countries/:cc/profile", {Access::Public}, [&env](Ctx& ctx) {
        std::string cc = ctx.param("cc");
        for (auto& ch : cc) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (!v::isValidCountryCode(cc)) throw notFound("country");
        const json& p = env.profiles.get(env.profiles.codeForCountry(cc));
        json taxes = json::array();
        for (const auto& t : p.at("tax_rates"))
            taxes.push_back({{"code", t["code"]}, {"label_fr", t["label_fr"]}, {"label_en", t["label_en"]},
                             {"rate_bp", t["rate_bp"]}});
        ctx.json(200, {{"profile", p.at("code")},
                       {"version", p.at("version")},
                       {"default_currency", p.value("default_currency", "USD")},
                       {"legal_ids", p.at("legal_ids")},
                       {"tax_rates", taxes},
                       {"document_notices", p.value("document_notices", json::object())},
                       {"validation_status", p.value("validation_status", "")}});
    });
}

void registerEstablishmentRoutes(App& app, Env& env) {
    // ---------------------------------------------------------------- creation (option B)
    app.post("/api/v1/establishments", {Access::Public}, [&env](Ctx& ctx) {
        throttle(env.pool, "create:ip:" + ctx.actor.ip, 10, 60);
        const auto& b = ctx.body();
        std::string kind = in::reqEnum(b, "kind", {"individual", "company"});
        std::string tradeName = in::reqStr(b, "trade_name", 200);
        auto legalName = in::optStr(b, "legal_name", 200);
        std::string country = in::reqStr(b, "country_code", 2);
        for (auto& ch : country) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (!v::isValidCountryCode(country)) throw invalid("country_code", "invalid_country");
        auto city = in::optStr(b, "city", 100);
        auto address = in::optStr(b, "address", 300);
        auto sector = in::optStr(b, "sector", 100);
        std::string lang = in::optEnum(b, "preferred_language", {"fr", "en"}).value_or("fr");
        std::string currency = in::reqCurrency(b, "main_currency");
        auto email = in::optEmail(b, "email");
        auto phone = in::optPhone(b, "phone");
        auto website = in::optStr(b, "website", 200);
        bool vatRegistered = in::optBool(b, "vat_registered", false);
        if (!b.contains("owner") || !b["owner"].is_object()) throw invalid("owner", "required");
        const json& o = b["owner"];
        std::string ownerName = in::reqStr(o, "full_name", 120);
        std::string ownerEmail = in::reqEmail(o, "email");
        auto ownerPhone = in::optPhone(o, "phone");
        if (!o.contains("password") || !o["password"].is_string()) throw invalid("owner.password", "required");
        std::string password = o["password"];

        std::string profileCode = env.profiles.codeForCountry(country);
        const json& profile = env.profiles.get(profileCode);
        auto ids = parseLegalIds(b, profile, kind, vatRegistered);
        std::string timezone = in::optStr(b, "timezone", 64).value_or(profile.value("default_timezone", "UTC"));

        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            if (!tx.opt("SELECT 1 FROM currencies WHERE code = $1 AND active", currency))
                throw invalid("main_currency", "unsupported_currency");
            if (!tx.opt("SELECT 1 FROM pg_timezone_names WHERE name = $1", timezone)) throw invalid("timezone", "invalid_timezone");
            // Duplicate prevention on legal identifiers. The unique index is the
            // final guard against two simultaneous creations.
            for (const auto& id : ids) {
                if (tx.opt("SELECT 1 FROM establishment_identifiers WHERE country_code = $1 AND kind = $2 "
                           "AND normalized_value = $3",
                           country, id.kind, id.normalized))
                    throw conflict("establishment_exists",
                                   "an establishment with this identifier already exists",
                                   {{"identifier", id.kind}, {"recovery", "attachment_request"}});
            }
            // Owner account: new user, or an existing user proving ownership with its password.
            std::string userId;
            if (auto u = tx.opt("SELECT id, password_hash, must_change_password FROM users WHERE email = $1", ownerEmail)) {
                if (!crypto::verifyPassword(db::str((*u)[1]), password)) throw conflict("email_taken");
                // A temporary password (given by an administrator) is replaced before any other use.
                if (db::boolean((*u)[2])) throw conflict("password_change_required");
                userId = db::str((*u)[0]);
            } else {
                auto issue = v::checkPassword(password, ownerEmail);
                if (issue != v::PasswordIssue::None) throw invalid("owner.password", v::passwordIssueCode(issue));
                userId = db::str(tx.one("INSERT INTO users (email, password_hash, full_name, phone, preferred_language) "
                                        "VALUES ($1, $2, $3, $4, $5) RETURNING id",
                                        ownerEmail, crypto::hashPassword(password, env.cfg.argon2), ownerName,
                                        ownerPhone, lang)[0]);
            }
            std::string est = db::str(tx.one(
                "INSERT INTO establishments (kind, trade_name, legal_name, country_code, city, address, sector, "
                "preferred_language, main_currency, email, phone, website, vat_registered, compliance_profile_code, timezone) "
                "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15) RETURNING id",
                kind, tradeName, legalName, country, city, address, sector, lang, currency, email, phone, website,
                vatRegistered, profileCode, timezone)[0]);
            tx.setTenant(est);
            for (const auto& id : ids)
                tx.q("INSERT INTO establishment_identifiers (establishment_id, kind, country_code, value, "
                     "normalized_value) VALUES ($1,$2,$3,$4,$5)",
                     est, id.kind, country, id.value, id.normalized);
            tx.q("INSERT INTO memberships (establishment_id, user_id, role) VALUES ($1, $2, 'owner')", est, userId);
            seedEstablishment(env, tx, est, profile, vatRegistered, lang, currency);
            std::string code = codes::issue(env, tx, est, "system", std::nullopt);

            // Free trial, once per identity: legal identifiers, owner e-mail and phone.
            std::vector<std::string> prints;
            for (const auto& id : ids) prints.push_back(fingerprint(env, "id:" + country + ":" + id.kind + ":" + id.normalized));
            prints.push_back(fingerprint(env, "email:" + ownerEmail));
            if (ownerPhone) prints.push_back(fingerprint(env, "phone:" + *ownerPhone));
            bool trialUsed = false;
            for (const auto& p : prints)
                if (tx.opt("SELECT 1 FROM trial_registry WHERE fingerprint = decode($1,'hex')", p)) trialUsed = true;
            int trialDays = setting(tx, "trial_days", 30);
            std::string subId;
            if (!trialUsed) {
                subId = db::str(tx.one(
                    "INSERT INTO subscriptions (establishment_id, status, is_trial, period_start, period_end) "
                    "VALUES ($1, 'trialing', true, now(), now() + make_interval(days => $2)) RETURNING id",
                    est, trialDays)[0]);
                for (const auto& p : prints)
                    tx.q("INSERT INTO trial_registry (fingerprint, establishment_id) VALUES (decode($1,'hex'), $2) "
                         "ON CONFLICT DO NOTHING",
                         p, est);
            } else {
                subId = db::str(tx.one(
                    "INSERT INTO subscriptions (establishment_id, status, is_trial, period_start, period_end) "
                    "VALUES ($1, 'suspended', false, now(), now()) RETURNING id",
                    est)[0]);
            }
            tx.q("INSERT INTO subscription_events (subscription_id, establishment_id, kind, to_status, details, "
                 "actor_type, actor_id) VALUES ($1, $2, $3, $4, $5::jsonb, 'user', $6)",
                 subId, est, trialUsed ? "trial_refused" : "trial_started", trialUsed ? "suspended" : "trialing",
                 json{{"trial_days", trialUsed ? 0 : trialDays}}.dump(), userId);
            Actor a = ctx.actor;
            a.userId = userId;
            a.establishmentId = est;
            audit(tx, a, "establishment.created", "establishment", est,
                  {{"kind", kind}, {"country", country}, {"trial_granted", !trialUsed}});
            json session = createSession(env, tx, userId, est, ctx.actor);
            return {{"establishment", {{"id", est}, {"trade_name", tradeName}, {"kind", kind}, {"profile", profileCode}}},
                    {"establishment_code", code},
                    {"trial_granted", !trialUsed},
                    {"subscription", subscriptions::view(tx, est)},
                    {"session", session}};
        });
        ctx.json(201, out);
    });

    // A person whose business already exists asks to be attached. Handled by
    // the owner or by support after identity verification; never automatic.
    app.post("/api/v1/establishments/attachment-requests", {Access::Public}, [&env](Ctx& ctx) {
        throttle(env.pool, "attach:ip:" + ctx.actor.ip, 5, 60);
        const auto& b = ctx.body();
        std::string country = in::reqStr(b, "country_code", 2);
        std::string kind = in::reqStr(b, "identifier_kind", 24);
        auto norm = v::normalizeLegalId(in::reqStr(b, "identifier_value", 64));
        std::string name = in::reqStr(b, "full_name", 120);
        std::string email = in::reqEmail(b, "email");
        auto phone = in::optPhone(b, "phone");
        auto message = in::optStr(b, "message", 1000);
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            if (!norm) return;
            auto e = tx.opt("SELECT establishment_id FROM establishment_identifiers WHERE country_code = $1 AND kind = $2 "
                            "AND normalized_value = $3",
                            country, kind, *norm);
            if (!e) return;
            std::string est = db::str((*e)[0]);
            auto id = db::str(tx.one("INSERT INTO attachment_requests (establishment_id, requester_name, requester_email, "
                                     "requester_phone, message) VALUES ($1,$2,$3,$4,$5) RETURNING id",
                                     est, name, email, phone, message)[0]);
            notify(tx, est, std::nullopt, "attachment_request", {{"request_id", id}, {"name", name}, {"email", email}});
            auditSystem(tx, est, "establishment.attachment_requested", "attachment_request", id, {{"email", email}},
                        "anonymous");
        });
        // Same answer whether or not the identifier exists.
        ctx.json(202, {{"status", "received"}});
    });

    // ---------------------------------------------------------------- join (option A)
    app.post("/api/v1/join/check-code", {Access::Public}, [&env](Ctx& ctx) {
        int limit = db::run(env.pool, std::nullopt, [](db::Tx& tx) { return setting(tx, "code_check_max_per_15min", 10); });
        throttle(env.pool, "code:ip:" + ctx.actor.ip, limit);
        auto canonical = nhill::code::normalize(in::reqStr(ctx.body(), "code", 40));
        const ApiError generic(422, "code_unusable", "invalid, expired or revoked code");
        if (!canonical) throw generic;
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto est = codes::resolve(env, tx, *canonical);
            if (!est) return nullptr;
            auto e = tx.one("SELECT trade_name, kind, logo_object_id IS NOT NULL FROM establishments WHERE id = $1", *est);
            std::string ticket = crypto::randomToken(32);
            tx.q("INSERT INTO join_tickets (ticket_hash, establishment_id, expires_at) "
                 "VALUES (decode($1,'hex'), $2, now() + interval '10 minutes')",
                 crypto::sha256Hex(ticket), *est);
            return {{"join_ticket", ticket},
                    {"expires_in_seconds", 600},
                    {"establishment", {{"trade_name", db::str(e[0])}, {"kind", db::str(e[1])}}}};
        });
        if (out.is_null()) {
            db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
                auditSystem(tx, std::nullopt, "join.code_rejected", "establishment_code", "", {{"ip", ctx.actor.ip}},
                            "anonymous");
            });
            throw generic;
        }
        ctx.json(200, out);
    });

    auto ticketEstablishment = [&env](const std::string& ticket) {
        auto est = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> std::optional<std::string> {
            auto r = tx.opt("SELECT establishment_id FROM join_tickets WHERE ticket_hash = decode($1,'hex') "
                            "AND expires_at > now()",
                            crypto::sha256Hex(ticket));
            if (!r) return std::nullopt;
            return db::str((*r)[0]);
        });
        if (!est) throw unauthorized("join_ticket_expired");
        return *est;
    };

    app.post("/api/v1/join/login", {Access::Public}, [&env, ticketEstablishment](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string est = ticketEstablishment(in::reqStr(b, "join_ticket", 100));
        std::string email = in::reqEmail(b, "email");
        if (!b.contains("password") || !b["password"].is_string()) throw invalid("password", "required");
        json session = loginForEstablishment(env, ctx, email, b["password"].get<std::string>(), est);
        ctx.json(200, session);
    });

    app.post("/api/v1/join/activate", {Access::Public}, [&env, ticketEstablishment](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string est = ticketEstablishment(in::reqStr(b, "join_ticket", 100));
        std::string email = in::reqEmail(b, "email");
        // Shown to the owner as "1234-5678": only the digits matter.
        std::string activation;
        for (char c : in::reqStr(b, "activation_code", 20))
            if (c >= '0' && c <= '9') activation.push_back(c);
        if (!b.contains("password") || !b["password"].is_string()) throw invalid("password", "required");
        std::string password = b["password"];
        throttle(env.pool, "activate:" + est + ":" + email, 6);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto inv = tx.opt(
                "SELECT id, role, encode(activation_hash,'hex') FROM invitations WHERE establishment_id = $1 "
                "AND email = $2 AND accepted_at IS NULL AND revoked_at IS NULL AND expires_at > now() FOR UPDATE",
                est, email);
            if (!inv || !crypto::constantTimeEquals(db::str((*inv)[2]), crypto::sha256Hex(activation)))
                return {{"_failed", true}};
            std::string userId;
            if (auto u = tx.opt("SELECT id, password_hash, must_change_password FROM users WHERE email = $1", email)) {
                // Existing account (member elsewhere): its own password proves identity.
                if (!crypto::verifyPassword(db::str((*u)[1]), password)) return {{"_failed", true}};
                // A temporary password (given by an administrator) is replaced before any other use.
                if (db::boolean((*u)[2])) throw conflict("password_change_required");
                userId = db::str((*u)[0]);
            } else {
                std::string name = in::reqStr(b, "full_name", 120);
                auto issue = v::checkPassword(password, email);
                if (issue != v::PasswordIssue::None) throw invalid("password", v::passwordIssueCode(issue));
                userId = db::str(tx.one("INSERT INTO users (email, password_hash, full_name, preferred_language) "
                                        "VALUES ($1, $2, $3, $4) RETURNING id",
                                        email, crypto::hashPassword(password, env.cfg.argon2), name, ctx.actor.lang)[0]);
            }
            tx.q("INSERT INTO memberships (establishment_id, user_id, role) VALUES ($1, $2, $3) "
                 "ON CONFLICT (establishment_id, user_id) DO UPDATE SET role = EXCLUDED.role, status = 'active'",
                 est, userId, db::str((*inv)[1]));
            tx.q("UPDATE invitations SET accepted_at = now() WHERE id = $1", db::str((*inv)[0]));
            tx.q("DELETE FROM join_tickets WHERE ticket_hash = decode($1,'hex')",
                 crypto::sha256Hex(b["join_ticket"].get<std::string>()));
            Actor a = ctx.actor;
            a.userId = userId;
            a.establishmentId = est;
            audit(tx, a, "member.activated", "membership", userId, {{"role", db::str((*inv)[1])}});
            notify(tx, est, std::nullopt, "member_joined", {{"email", email}});
            return createSession(env, tx, userId, est, ctx.actor);
        });
        if (out.contains("_failed")) throw unauthorized("activation_failed");
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- profile
    app.get("/api/v1/establishment", {Access::User}, [&env](Ctx& ctx) {
        ctx.json(200, db::run(env.pool, ctx.actor.establishmentId,
                              [&](db::Tx& tx) { return establishmentJson(env, tx, ctx.actor.establishmentId); }));
    });

    app.patch("/api/v1/establishment", {Access::User, "settings.manage", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        auto tradeName = in::optStr(b, "trade_name", 200);
        auto legalName = in::optStr(b, "legal_name", 200);
        auto city = in::optStr(b, "city", 100);
        auto address = in::optStr(b, "address", 300);
        auto sector = in::optStr(b, "sector", 100);
        auto lang = in::optEnum(b, "preferred_language", {"fr", "en"});
        auto email = in::optEmail(b, "email");
        auto phone = in::optPhone(b, "phone");
        auto website = in::optStr(b, "website", 200);
        std::optional<bool> vat;
        if (b.contains("vat_registered")) vat = in::optBool(b, "vat_registered", false);
        auto timezone = in::optStr(b, "timezone", 64);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            if (timezone && !tx.opt("SELECT 1 FROM pg_timezone_names WHERE name = $1", *timezone))
                throw invalid("timezone", "invalid_timezone");
            tx.q("UPDATE establishments SET trade_name = COALESCE($2, trade_name), legal_name = COALESCE($3, legal_name), "
                 "city = COALESCE($4, city), address = COALESCE($5, address), sector = COALESCE($6, sector), "
                 "preferred_language = COALESCE($7, preferred_language), email = COALESCE($8, email), "
                 "phone = COALESCE($9, phone), website = COALESCE($10, website), "
                 "vat_registered = COALESCE($11, vat_registered), timezone = COALESCE($12, timezone) WHERE id = $1",
                 ctx.actor.establishmentId, tradeName, legalName, city, address, sector, lang, email, phone, website, vat,
                 timezone);
            audit(tx, ctx.actor, "establishment.updated", "establishment", ctx.actor.establishmentId, b);
            return establishmentJson(env, tx, ctx.actor.establishmentId);
        });
        ctx.json(200, out);
    });

    app.put("/api/v1/establishment/legal-ids", {Access::User, "settings.manage", true}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            auto e = tx.one("SELECT country_code, compliance_profile_code, kind, vat_registered FROM establishments WHERE id = $1",
                            ctx.actor.establishmentId);
            std::string country = db::str(e[0]);
            auto ids = parseLegalIds(ctx.body(), env.profiles.get(db::str(e[1])), db::str(e[2]), db::boolean(e[3]));
            tx.q("DELETE FROM establishment_identifiers WHERE establishment_id = $1", ctx.actor.establishmentId);
            for (const auto& id : ids)
                tx.q("INSERT INTO establishment_identifiers (establishment_id, kind, country_code, value, normalized_value) "
                     "VALUES ($1,$2,$3,$4,$5)",
                     ctx.actor.establishmentId, id.kind, country, id.value, id.normalized);
            audit(tx, ctx.actor, "establishment.legal_ids_updated", "establishment", ctx.actor.establishmentId,
                  ctx.body());
            return establishmentJson(env, tx, ctx.actor.establishmentId);
        });
        ctx.json(200, out);
    });

    app.patch("/api/v1/establishment/settings", {Access::User, "settings.manage", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::optional<bool> neg;
        if (b.contains("allow_negative_stock")) neg = in::optBool(b, "allow_negative_stock", false);
        auto rounding = in::optEnum(b, "tax_rounding", {"per_line", "per_rate"});
        std::optional<int> terms;
        if (b.contains("default_payment_terms_days")) terms = in::optInt(b, "default_payment_terms_days", 0, 0, 365);
        auto docLang = in::optEnum(b, "document_language", {"fr", "en"});
        std::optional<std::string> footer, payInfo;
        if (b.contains("invoice_footer")) footer = in::optStr(b, "invoice_footer", 500).value_or("");
        if (b.contains("payment_instructions")) payInfo = in::optStr(b, "payment_instructions", 500).value_or("");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            tx.q("UPDATE establishment_settings SET allow_negative_stock = COALESCE($2, allow_negative_stock), "
                 "tax_rounding = COALESCE($3, tax_rounding), default_payment_terms_days = COALESCE($4, "
                 "default_payment_terms_days), document_language = COALESCE($5, document_language), "
                 "invoice_footer = COALESCE($6, invoice_footer), payment_instructions = COALESCE($7, payment_instructions), "
                 "updated_at = now() WHERE establishment_id = $1",
                 ctx.actor.establishmentId, neg, rounding, terms, docLang, footer, payInfo);
            audit(tx, ctx.actor, "establishment.settings_updated", "establishment", ctx.actor.establishmentId, b);
            return establishmentJson(env, tx, ctx.actor.establishmentId);
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- logo
    app.put("/api/v1/establishment/logo", {Access::User, "settings.manage", true}, [&env](Ctx& ctx) {
        ImageInfo info = validateImage(ctx.req.body, ctx.header("Content-Type"));
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            auto old = tx.one("SELECT logo_object_id FROM establishments WHERE id = $1", ctx.actor.establishmentId);
            std::string id = db::str(tx.one(
                "INSERT INTO objects (establishment_id, purpose, mime, byte_size, sha256, width, height, data, created_by) "
                "VALUES ($1, 'logo', $2, $3, $4, $5, $6, decode($7,'hex'), $8) RETURNING id",
                ctx.actor.establishmentId, info.mime, static_cast<int>(ctx.req.body.size()),
                crypto::sha256Hex(ctx.req.body), info.width, info.height, crypto::toHex(ctx.req.body),
                ctx.actor.userId)[0]);
            tx.q("UPDATE establishments SET logo_object_id = $2 WHERE id = $1", ctx.actor.establishmentId, id);
            if (!old[0].is_null()) tx.q("DELETE FROM objects WHERE id = $1", db::str(old[0]));
            audit(tx, ctx.actor, "establishment.logo_updated", "object", id,
                  {{"mime", info.mime}, {"width", info.width}, {"height", info.height}});
        });
        ctx.json(200, {{"mime", info.mime}, {"width", info.width}, {"height", info.height}});
    });

    app.get("/api/v1/establishment/logo", {Access::User}, [&env](Ctx& ctx) {
        auto row = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return tx.opt("SELECT o.mime, encode(o.data,'hex') FROM establishments e JOIN objects o "
                          "ON o.id = e.logo_object_id WHERE e.id = $1",
                          ctx.actor.establishmentId);
        });
        if (!row) throw notFound("logo");
        ctx.res.set_header("Cache-Control", "private, max-age=300");
        ctx.file(*crypto::fromHex(db::str((*row)[1])), db::str((*row)[0]), "logo", true);
    });

    app.del("/api/v1/establishment/logo", {Access::User, "settings.manage", true}, [&env](Ctx& ctx) {
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            auto old = tx.one("SELECT logo_object_id FROM establishments WHERE id = $1", ctx.actor.establishmentId);
            tx.q("UPDATE establishments SET logo_object_id = NULL WHERE id = $1", ctx.actor.establishmentId);
            if (!old[0].is_null()) tx.q("DELETE FROM objects WHERE id = $1", db::str(old[0]));
            audit(tx, ctx.actor, "establishment.logo_removed", "establishment", ctx.actor.establishmentId);
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- establishment code
    app.get("/api/v1/establishment/code", {Access::User, "code.view"}, [&env](Ctx& ctx) {
        auto cur = db::run(env.pool, ctx.actor.establishmentId,
                           [&](db::Tx& tx) { return codes::current(env, tx, ctx.actor.establishmentId); });
        if (!cur) throw notFound("establishment code");
        ctx.json(200, {{"code", cur->display}, {"created_at", cur->createdAt}, {"expires_at", cur->expiresAt}});
    });

    app.post("/api/v1/establishment/code/rotate", {Access::User, "code.rotate"}, [&env](Ctx& ctx) {
        auto reason = in::optStr(ctx.body(), "reason", 200).value_or("manual");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            std::string code = codes::rotate(env, tx, ctx.actor.establishmentId, "revoked", reason, "user", ctx.actor.userId);
            audit(tx, ctx.actor, "establishment.code_rotated", "establishment_code", "", {{"reason", reason}});
            notify(tx, ctx.actor.establishmentId, std::nullopt, "code_rotated", {{"by", ctx.actor.fullName}});
            auto cur = codes::current(env, tx, ctx.actor.establishmentId);
            return {{"code", code}, {"created_at", cur->createdAt}, {"expires_at", cur->expiresAt}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/establishment/audit", {Access::User, "audit.read"}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(
                     "SELECT a.id, a.actor_type, COALESCE(u.full_name, ''), a.action, a.entity, a.entity_id, a.details, "
                     "nh_iso(a.created_at) FROM audit_log a LEFT JOIN users u ON a.actor_type = 'user' AND u.id = a.actor_id "
                     "WHERE a.establishment_id = $1 ORDER BY a.id DESC LIMIT $2 OFFSET $3",
                     ctx.actor.establishmentId, pg.limit, pg.offset))
                items.push_back({{"id", r[0].as<int64_t>()}, {"actor_type", db::str(r[1])}, {"actor", db::str(r[2])},
                                 {"action", db::str(r[3])}, {"entity", db::str(r[4])}, {"entity_id", db::str(r[5])},
                                 {"details", json::parse(db::str(r[6]))}, {"created_at", db::str(r[7])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });
}

}  // namespace nh
