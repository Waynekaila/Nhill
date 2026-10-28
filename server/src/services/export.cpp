// Full data export of an establishment for its owner (decision of 28/10/2026, option 1).
//
// The owner keeps, at any time and whatever the plan or the state of the subscription
// (read-only included), a complete archive of the establishment's data, data of features
// that are no longer in the plan included. It is one general action of the owner's
// settings, never a button of the removed feature: the plan rules of the applications
// (a feature outside the plan stays invisible) are unchanged.
//
// Why: the data belong to the customer; leaving a service or changing plan must not take
// them hostage. This follows the European references the platform uses (GDPR art. 20,
// portability; Regulation (EU) 2023/2854 "Data Act", chapter VI, switching between data
// processing services). Using them as a reference is a design choice, not a statement
// that Nhill is subject to, or compliant with, those texts.
//
// Format (docs/*/export-des-donnees.md): a ZIP archive with
//   README.txt            content and conventions (FR/EN);
//   manifest.json         establishment, generation date, author, tables and row counts,
//                         SHA-256 of every file;
//   json/<table>.json     exact values (array of objects), machine-readable;
//   csv/<table>.csv       the same rows for spreadsheets (UTF-8 with BOM, ';', values
//                         starting with = + - @ prefixed with ' against formula injection);
//   files/<purpose>/...   stored files (logo, expense receipts).
// Never exported: secrets and their hashes (passwords, tokens, codes), internal notes of
// the support team, technical tables (sessions, idempotency keys, e-mail queue).
#include <algorithm>
#include <ctime>

#include "nhill/version.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"
#include "util/csv.hpp"
#include "util/zip.hpp"

namespace nh {

namespace {

struct Table {
    const char* name;
    // Custom query (parameter $1 = establishment id); empty = every column of the table
    // that is not excluded, filtered on establishment_id.
    const char* sql = "";
};

// Order: reference data first, then documents, then the history.
const std::vector<Table>& tables() {
    static const std::vector<Table> t = {
        {"establishments", ""},
        {"establishment_identifiers"},
        {"establishment_settings"},
        {"members",
         "SELECT u.id AS user_id, u.full_name, u.email, u.phone, m.role, m.status, m.created_at FROM memberships m "
         "JOIN users u ON u.id = m.user_id WHERE m.establishment_id = $1 ORDER BY m.created_at"},
        {"invitations"},
        {"tax_rates"},
        {"product_categories"},
        {"products"},
        {"warehouses"},
        {"stock_levels"},
        {"stock_movements"},
        {"parties"},
        {"sales_documents"},
        {"sales_document_lines"},
        {"document_status_history"},
        {"document_sequences"},
        {"purchases"},
        {"purchase_lines"},
        {"cash_accounts"},
        {"expense_categories"},
        {"expenses"},
        {"payments"},
        {"exchange_rates"},
        {"pos_registers"},
        {"pos_register_users"},
        {"pos_payment_methods"},
        {"pos_settings"},
        {"pos_sessions"},
        {"pos_cash_movements"},
        {"pos_approvals"},
        {"sales_promo_codes"},
        {"sales_promo_redemptions"},
        {"subscriptions"},
        {"subscription_events"},
        {"subscription_orders"},
        {"support_tickets",
         "SELECT id, number, created_by, source, category, severity, subject, status, app_version, platform, created_at, "
         "updated_at, last_message_at FROM support_tickets WHERE establishment_id = $1 ORDER BY number"},
        {"support_ticket_messages",
         "SELECT m.id, m.ticket_id, m.author_type, CASE WHEN m.author_type = 'user' THEN m.author_id END AS author_id, "
         "m.body, m.created_at FROM support_ticket_messages m JOIN support_tickets t ON t.id = m.ticket_id "
         "WHERE t.establishment_id = $1 AND NOT m.internal ORDER BY m.id"},
        {"audit_log"},
        {"objects"},
    };
    return t;
}

bool excludedColumn(const std::string& name, const std::string& type) {
    if (type == "bytea") return true;
    for (const char* bad : {"hash", "secret", "sealed", "token", "password"})
        if (name.find(bad) != std::string::npos) return true;
    return false;
}

// Identifiers come from the catalogue of the database or from this file; quoted anyway.
std::string ident(const std::string& n) {
    std::string out = "\"";
    for (char c : n) out += c == '"' ? std::string("\"\"") : std::string(1, c);
    return out + "\"";
}

std::string cell(const json& v) {
    if (v.is_null()) return "";
    if (v.is_string()) return v.get<std::string>();
    if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
    return v.dump();
}

std::string isoNow() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string slug(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
        else if (!out.empty() && out.back() != '-') out.push_back('-');
        if (out.size() >= 40) break;
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "etablissement" : out;
}

const char* kReadme =
    "NHILL - EXPORT COMPLET DES DONNEES / FULL DATA EXPORT\r\n"
    "=====================================================\r\n\r\n"
    "FR\r\n"
    "Cette archive contient toutes les donnees de l'etablissement enregistrees dans Nhill a la date indiquee dans\r\n"
    "manifest.json, y compris celles des fonctionnalites qui ne font plus partie de la formule.\r\n"
    "- json/<table>.json : valeurs exactes (tableau d'objets), pour un traitement automatique ou un import.\r\n"
    "- csv/<table>.csv   : memes lignes pour un tableur (UTF-8, separateur point-virgule). Une valeur commencant\r\n"
    "                      par = + - ou @ est precedee d'une apostrophe pour eviter l'execution de formules.\r\n"
    "- files/            : fichiers stockes (logo, justificatifs de depenses).\r\n"
    "- manifest.json     : liste des fichiers, nombre de lignes et empreinte SHA-256 de chaque fichier.\r\n"
    "Conventions : montants en unites mineures (colonnes *_minor, ex. 150000 = 1 500,00) avec leur devise ;\r\n"
    "quantites en milliemes (colonnes *_milli) ; dates et heures en ISO 8601 (UTC sauf indication) ;\r\n"
    "taux en points de base (colonnes *_bp, 1600 = 16 %).\r\n"
    "Ne sont jamais exportes : mots de passe, jetons, codes et leurs empreintes, notes internes du support.\r\n"
    "Conservez cette archive en lieu sur : elle contient des donnees personnelles (clients, fournisseurs, equipe).\r\n\r\n"
    "EN\r\n"
    "This archive contains all the data of the establishment stored in Nhill at the date given in manifest.json,\r\n"
    "including the data of features that are no longer part of the plan.\r\n"
    "- json/<table>.json : exact values (array of objects), for automated processing or import.\r\n"
    "- csv/<table>.csv   : the same rows for spreadsheets (UTF-8, semicolon separator). A value starting with\r\n"
    "                      = + - or @ is prefixed with an apostrophe so that no formula is executed.\r\n"
    "- files/            : stored files (logo, expense receipts).\r\n"
    "- manifest.json     : list of files, row counts and SHA-256 of every file.\r\n"
    "Conventions: amounts in minor units (*_minor columns, e.g. 150000 = 1,500.00) with their currency;\r\n"
    "quantities in thousandths (*_milli columns); dates and times in ISO 8601 (UTC unless stated);\r\n"
    "rates in basis points (*_bp columns, 1600 = 16%).\r\n"
    "Never exported: passwords, tokens, codes and their hashes, internal notes of the support team.\r\n"
    "Keep this archive in a safe place: it contains personal data (customers, suppliers, team).\r\n";

}  // namespace

void registerExportRoutes(App& app, Env& env) {
    // Read-only: allowed with a suspended or expired subscription, never limited by the plan.
    app.get("/api/v1/establishment/export", {Access::User}, [&env](Ctx& ctx) {
        if (ctx.actor.role != "owner") throw forbidden("owner_only");
        throttle(env.pool, "export:" + ctx.actor.establishmentId, 3, 15);
        std::string name, archive;
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            const std::string& est = ctx.actor.establishmentId;
            auto e = tx.one("SELECT trade_name FROM establishments WHERE id = $1", est);
            const std::string generatedAt = isoNow();
            zip::Writer z;
            json manifest = {{"format", "nhill-export"},
                             {"format_version", 1},
                             {"product_version", nhill::kProductVersion},
                             {"establishment", {{"id", est}, {"trade_name", db::str(e[0])}}},
                             {"generated_at", generatedAt},
                             {"generated_by", {{"user_id", ctx.actor.userId}, {"full_name", ctx.actor.fullName}}},
                             {"tables", json::array()},
                             {"files", json::array()}};
            auto addFile = [&](const std::string& path, const std::string& bytes) {
                z.add(path, bytes);
                manifest["files"].push_back({{"path", path}, {"bytes", bytes.size()}, {"sha256", crypto::sha256Hex(bytes)}});
            };
            for (const auto& t : tables()) {
                std::string sql = t.sql;
                std::vector<std::string> columns;
                if (sql.empty()) {
                    std::string list;
                    bool hasEst = false;
                    for (const auto& c : tx.q("SELECT column_name, data_type FROM information_schema.columns "
                                              "WHERE table_schema = 'nhill' AND table_name = $1 ORDER BY ordinal_position",
                                              std::string(t.name))) {
                        std::string col = db::str(c[0]);
                        if (col == "establishment_id") hasEst = true;
                        if (excludedColumn(col, db::str(c[1]))) continue;
                        columns.push_back(col);
                        list += (list.empty() ? "" : ", ") + ident(col);
                    }
                    const bool self = std::string(t.name) == "establishments";
                    if (columns.empty() || (!hasEst && !self)) continue;  // never exported without a tenant filter
                    sql = "SELECT " + list + " FROM " + ident(t.name) + (self ? " WHERE id = $1" : " WHERE establishment_id = $1");
                }
                json rows = json::array();
                for (const auto& r : tx.q("SELECT COALESCE(jsonb_agg(to_jsonb(x)), '[]'::jsonb) FROM (" + sql + ") x", est))
                    rows = json::parse(db::str(r[0]));
                if (columns.empty() && !rows.empty())
                    for (auto it = rows[0].begin(); it != rows[0].end(); ++it) columns.push_back(it.key());
                csv::Writer w;
                w.row(columns);
                for (const auto& row : rows) {
                    std::vector<std::string> cells;
                    cells.reserve(columns.size());
                    for (const auto& c : columns) cells.push_back(row.contains(c) ? cell(row[c]) : "");
                    w.row(cells);
                }
                addFile(std::string("json/") + t.name + ".json", rows.dump(1));
                addFile(std::string("csv/") + t.name + ".csv", w.str());
                manifest["tables"].push_back({{"name", t.name}, {"rows", rows.size()}});
            }
            for (const auto& o : tx.q("SELECT id, purpose, mime, encode(data, 'hex') FROM objects WHERE establishment_id = $1 "
                                      "ORDER BY created_at",
                                      est)) {
                std::string mime = db::str(o[2]);
                std::string ext = mime == "application/pdf" ? ".pdf" : mime == "image/png" ? ".png" : ".jpg";
                addFile("files/" + db::str(o[1]) + "/" + db::str(o[0]) + ext, *crypto::fromHex(db::str(o[3])));
            }
            z.add("README.txt", kReadme);
            z.add("manifest.json", manifest.dump(2));
            archive = z.finish();
            name = "nhill-export-" + slug(db::str(e[0])) + "-" + generatedAt.substr(0, 10) + ".zip";
            audit(tx, ctx.actor, "establishment.exported", "establishment", est,
                  {{"tables", manifest["tables"].size()}, {"files", manifest["files"].size()}, {"bytes", archive.size()}});
        });
        ctx.file(archive, "application/zip", name);
    });
}

}  // namespace nh
