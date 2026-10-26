#include "services/providers.hpp"

#include <algorithm>
#include <ctime>
#include <mutex>
#include <sstream>

#include "nhill/money.hpp"
#include "util/crypto.hpp"
#include "util/log.hpp"

namespace nh::providers {

namespace {

std::mutex gTransportMutex;
Transport gTransport;

struct Url {
    std::string scheme, host, path;
    int port = 443;
};

std::optional<Url> parseUrl(const std::string& u) {
    Url r;
    auto p = u.find("://");
    if (p == std::string::npos) return std::nullopt;
    r.scheme = u.substr(0, p);
    std::string rest = u.substr(p + 3);
    auto slash = rest.find('/');
    std::string hostport = rest.substr(0, slash);
    r.path = slash == std::string::npos ? "/" : rest.substr(slash);
    auto colon = hostport.find(':');
    r.host = hostport.substr(0, colon);
    r.port = r.scheme == "https" ? 443 : 80;
    if (colon != std::string::npos) r.port = std::stoi(hostport.substr(colon + 1));
    if (r.host.empty() || (r.scheme != "https" && r.scheme != "http")) return std::nullopt;
    return r;
}

HttpResponse realTransport(const std::string& method, const std::string& url, const httplib::Headers& headers,
                           const std::string& body, const std::string& contentType) {
    auto u = parseUrl(url);
    if (!u || u->scheme != "https") throw ApiError(502, "provider_unavailable", "provider URL must use HTTPS");
    httplib::SSLClient cli(u->host, u->port);
    cli.enable_server_certificate_verification(true);
    cli.set_connection_timeout(10, 0);
    cli.set_read_timeout(20, 0);
    cli.set_write_timeout(20, 0);
    httplib::Result res = method == "GET" ? cli.Get(u->path, headers) : cli.Post(u->path, headers, body, contentType);
    if (!res) {
        log::warn("payment provider call failed: " + u->host + " (" + httplib::to_string(res.error()) + ")");
        throw ApiError(502, "provider_unavailable", "payment provider unreachable");
    }
    return {res->status, res->body};
}

HttpResponse call(const std::string& method, const std::string& url, const httplib::Headers& headers,
                  const std::string& body = "", const std::string& contentType = "") {
    Transport t;
    {
        std::lock_guard<std::mutex> l(gTransportMutex);
        t = gTransport;
    }
    return t ? t(method, url, headers, body, contentType) : realTransport(method, url, headers, body, contentType);
}

std::string urlEncode(const std::string& s) {
    std::ostringstream o;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o << c;
        else {
            char buf[4];
            std::snprintf(buf, sizeof buf, "%%%02X", c);
            o << buf;
        }
    }
    return o.str();
}

std::string form(const std::vector<std::pair<std::string, std::string>>& kv) {
    std::string out;
    for (const auto& [k, v] : kv) {
        if (!out.empty()) out += '&';
        out += urlEncode(k) + "=" + urlEncode(v);
    }
    return out;
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    for (size_t p = 0; (p = s.find(from, p)) != std::string::npos; p += to.size()) s.replace(p, from.size(), to);
    return s;
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string setting(const Provider& p, const std::string& k, const std::string& def = "") {
    return p.settings.contains(k) && p.settings[k].is_string() ? p.settings[k].get<std::string>() : def;
}
std::string secret(const Provider& p, const std::string& k) {
    if (!p.secrets.contains(k) || !p.secrets[k].is_string() || p.secrets[k].get<std::string>().empty())
        throw ApiError(503, "provider_not_configured", "missing provider secret: " + k);
    return p.secrets[k].get<std::string>();
}

std::string returnUrl(const Env& env, const Provider& p, const std::string& key, const OrderInfo& o,
                      const std::string& outcome) {
    std::string u = setting(p, key);
    if (u.empty()) {
        if (env.cfg.publicBaseUrl.empty())
            throw ApiError(503, "provider_not_configured", "NH_PUBLIC_BASE_URL or " + key + " is required");
        u = env.cfg.publicBaseUrl + "/payment/return?order={order_id}&outcome=" + outcome;
    }
    return replaceAll(u, "{order_id}", o.id);
}

json parseJson(const HttpResponse& r) {
    try {
        return json::parse(r.body);
    } catch (...) {
        throw ApiError(502, "provider_unavailable", "invalid provider response");
    }
}

int64_t decimalToMinor(const std::string& value, const std::string& currency) {
    auto v = nhill::parseMoney(value, nhill::currencyMinorUnits(currency));
    if (!v) throw ApiError(422, "provider_amount_invalid", "invalid amount from provider");
    return *v;
}

// ------------------------------------------------------------------ generic HMAC
Checkout genericCheckout(Env&, const Provider& p, const OrderInfo& o) {
    std::string tmpl = setting(p, "checkout_url_template");
    if (tmpl.rfind("https://", 0) != 0) throw ApiError(503, "provider_not_configured", "checkout_url_template");
    std::string amount = money(o.amountMinor, o.currency);
    // The aggregator can check that the link was produced by Nhill (same secret as the webhook).
    std::string sig = crypto::hmacSha256Hex(secret(p, "webhook_secret"), o.id + "|" + amount + "|" + o.currency);
    std::string url = replaceAll(replaceAll(replaceAll(replaceAll(tmpl, "{order_id}", o.id), "{amount}", amount),
                                            "{currency}", o.currency),
                                 "{signature}", sig);
    return {o.id, url};
}

Event genericWebhook(Env&, const Provider& p, const httplib::Request& req) {
    std::string ts = req.get_header_value("X-Nhill-Timestamp");
    std::string sig = req.get_header_value("X-Nhill-Signature");
    long long t = 0;
    try {
        t = std::stoll(ts);
    } catch (...) {
        throw unauthorized("invalid_signature");
    }
    if (std::llabs(static_cast<long long>(std::time(nullptr)) - t) > 300) throw unauthorized("stale_signature");
    std::string expected = "v1=" + crypto::hmacSha256Hex(secret(p, "webhook_secret"), ts + "." + req.body);
    if (!crypto::constantTimeEquals(expected, sig)) throw unauthorized("invalid_signature");
    json b;
    try {
        b = json::parse(req.body);
    } catch (...) {
        throw badRequest("invalid_json");
    }
    Event e;
    e.eventId = in::reqStr(b, "event_id", 100);
    std::string type = in::reqEnum(b, "type", {"payment.succeeded", "payment.failed", "payment.refunded"});
    e.type = type.substr(8);
    json data = b.contains("data") && b["data"].is_object() ? b["data"] : json::object();
    e.orderId = in::reqUuid(data, "order_id");
    e.currency = in::reqCurrency(data, "currency");
    e.amountMinor = in::reqMoney(data, "amount", e.currency);
    e.reference = in::optStr(data, "provider_reference", 100).value_or("");
    return e;
}

// ------------------------------------------------------------------ Stripe
const char* kStripeApi = "https://api.stripe.com";

httplib::Headers stripeHeaders(const Provider& p, const std::string& idem = "") {
    httplib::Headers h{{"Authorization", "Bearer " + secret(p, "secret_key")}};
    if (!idem.empty()) h.emplace("Idempotency-Key", idem);
    return h;
}

Checkout stripeCheckout(Env& env, const Provider& p, const OrderInfo& o) {
    std::vector<std::pair<std::string, std::string>> kv = {
        {"mode", "payment"},
        {"client_reference_id", o.id},
        {"metadata[order_id]", o.id},
        {"payment_intent_data[metadata][order_id]", o.id},
        {"line_items[0][quantity]", "1"},
        {"line_items[0][price_data][currency]", lower(o.currency)},
        {"line_items[0][price_data][unit_amount]", std::to_string(o.amountMinor)},
        {"line_items[0][price_data][product_data][name]", o.description},
        {"success_url", returnUrl(env, p, "success_url", o, "success")},
        {"cancel_url", returnUrl(env, p, "cancel_url", o, "cancel")},
    };
    if (!o.customerEmail.empty()) kv.emplace_back("customer_email", o.customerEmail);
    auto r = call("POST", std::string(kStripeApi) + "/v1/checkout/sessions", stripeHeaders(p, "nhill-" + o.id),
                  form(kv), "application/x-www-form-urlencoded");
    json j = parseJson(r);
    if (r.status != 200 || !j.contains("id") || !j.contains("url") || !j["url"].is_string()) {
        log::warn("stripe: checkout session refused (HTTP " + std::to_string(r.status) + ")");
        throw ApiError(502, "provider_refused", "the payment provider refused the request");
    }
    return {j["id"].get<std::string>(), j["url"].get<std::string>()};
}

Event stripeWebhook(Env&, const Provider& p, const httplib::Request& req) {
    // Stripe-Signature: t=<unix>,v1=<hex>[,v1=...]  over "<t>.<raw body>" with the endpoint secret.
    std::string header = req.get_header_value("Stripe-Signature");
    std::string t;
    std::vector<std::string> v1;
    std::stringstream ss(header);
    std::string part;
    while (std::getline(ss, part, ',')) {
        auto eq = part.find('=');
        if (eq == std::string::npos) continue;
        std::string k = part.substr(0, eq), v = part.substr(eq + 1);
        if (k == "t") t = v;
        else if (k == "v1") v1.push_back(v);
    }
    long long ts = 0;
    try {
        ts = std::stoll(t);
    } catch (...) {
        throw unauthorized("invalid_signature");
    }
    if (std::llabs(static_cast<long long>(std::time(nullptr)) - ts) > 300) throw unauthorized("stale_signature");
    std::string expected = crypto::hmacSha256Hex(secret(p, "webhook_secret"), t + "." + req.body);
    bool ok = false;
    for (const auto& s : v1) ok = ok || crypto::constantTimeEquals(expected, s);
    if (!ok) throw unauthorized("invalid_signature");

    json ev;
    try {
        ev = json::parse(req.body);
    } catch (...) {
        throw badRequest("invalid_json");
    }
    Event e;
    e.eventId = ev.value("id", "");
    std::string type = ev.value("type", "");
    const json obj = ev.contains("data") && ev["data"].contains("object") ? ev["data"]["object"] : json::object();
    if (type != "checkout.session.completed" && type != "checkout.session.async_payment_succeeded" &&
        type != "checkout.session.async_payment_failed") {
        e.type = "ignored";
        e.detail = type;
        return e;
    }
    std::string sessionId = obj.value("id", "");
    if (sessionId.empty() || sessionId.find('/') != std::string::npos) throw badRequest("invalid_event");
    // Re-read the session from the API (source of truth) before acting on it.
    auto r = call("GET", std::string(kStripeApi) + "/v1/checkout/sessions/" + urlEncode(sessionId), stripeHeaders(p));
    json s = parseJson(r);
    if (r.status != 200) throw ApiError(502, "provider_unavailable", "cannot read the checkout session");
    e.orderId = s.contains("metadata") && s["metadata"].is_object() ? s["metadata"].value("order_id", "") : "";
    if (e.orderId.empty()) e.orderId = s.value("client_reference_id", "");
    e.currency = upper(s.value("currency", ""));
    e.amountMinor = s.value("amount_total", static_cast<int64_t>(-1));
    e.reference = s.contains("payment_intent") && s["payment_intent"].is_string() ? s["payment_intent"].get<std::string>()
                                                                                  : sessionId;
    std::string paymentStatus = s.value("payment_status", "");
    if (type == "checkout.session.async_payment_failed") e.type = "failed";
    else if (paymentStatus == "paid" || paymentStatus == "no_payment_required") e.type = "succeeded";
    else {
        e.type = "ignored";  // e.g. delayed method still processing: wait for async_payment_succeeded
        e.detail = "payment_status=" + paymentStatus;
    }
    return e;
}

// ------------------------------------------------------------------ PayPal (Orders v2)
std::string paypalBase(const Provider& p) {
    return p.environment == "live" ? "https://api-m.paypal.com" : "https://api-m.sandbox.paypal.com";
}

std::string base64(const std::string& in) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        uint32_t n = (uint8_t(in[i]) << 16) | (uint8_t(in[i + 1]) << 8) | uint8_t(in[i + 2]);
        out += t[n >> 18]; out += t[(n >> 12) & 63]; out += t[(n >> 6) & 63]; out += t[n & 63];
    }
    if (i + 1 == in.size()) {
        uint32_t n = uint8_t(in[i]) << 16;
        out += t[n >> 18]; out += t[(n >> 12) & 63]; out += "==";
    } else if (i + 2 == in.size()) {
        uint32_t n = (uint8_t(in[i]) << 16) | (uint8_t(in[i + 1]) << 8);
        out += t[n >> 18]; out += t[(n >> 12) & 63]; out += t[(n >> 6) & 63]; out += '=';
    }
    return out;
}

std::string paypalToken(const Provider& p) {
    std::string basic = setting(p, "client_id") + ":" + secret(p, "client_secret");
    std::string b64 = base64(basic);
    auto r = call("POST", paypalBase(p) + "/v1/oauth2/token", {{"Authorization", "Basic " + b64}},
                  "grant_type=client_credentials", "application/x-www-form-urlencoded");
    json j = parseJson(r);
    if (r.status != 200 || !j.contains("access_token")) throw ApiError(502, "provider_refused", "PayPal authentication failed");
    return j["access_token"].get<std::string>();
}

Checkout paypalCheckout(Env& env, const Provider& p, const OrderInfo& o) {
    std::string token = paypalToken(p);
    json body = {{"intent", "CAPTURE"},
                 {"purchase_units",
                  {{{"reference_id", o.id},
                    {"custom_id", o.id},
                    {"description", o.description.substr(0, 127)},
                    {"amount", {{"currency_code", o.currency}, {"value", money(o.amountMinor, o.currency)}}}}}},
                 {"payment_source",
                  {{"paypal",
                    {{"experience_context",
                      {{"return_url", returnUrl(env, p, "return_url", o, "success")},
                       {"cancel_url", returnUrl(env, p, "cancel_url", o, "cancel")},
                       {"user_action", "PAY_NOW"},
                       {"brand_name", setting(p, "brand_name", "Nhill")}}}}}}}};
    auto r = call("POST", paypalBase(p) + "/v2/checkout/orders",
                  {{"Authorization", "Bearer " + token}, {"PayPal-Request-Id", "nhill-" + o.id}}, body.dump(),
                  "application/json");
    json j = parseJson(r);
    if ((r.status != 200 && r.status != 201) || !j.contains("id")) throw ApiError(502, "provider_refused", "PayPal refused the order");
    std::string url;
    for (const auto& l : j.value("links", json::array()))
        if (l.value("rel", "") == "payer-action" || l.value("rel", "") == "approve") url = l.value("href", "");
    if (url.empty()) throw ApiError(502, "provider_refused", "PayPal returned no approval link");
    return {j["id"].get<std::string>(), url};
}

Event paypalCapture(const Provider& p, const std::string& token, const std::string& paypalOrderId, Event e) {
    auto r = call("POST", paypalBase(p) + "/v2/checkout/orders/" + urlEncode(paypalOrderId) + "/capture",
                  {{"Authorization", "Bearer " + token}, {"PayPal-Request-Id", "nhill-capture-" + paypalOrderId}}, "{}",
                  "application/json");
    json j = parseJson(r);
    if (r.status != 200 && r.status != 201) {
        // Already captured (e.g. webhook retried): the PAYMENT.CAPTURE.COMPLETED event applies the order.
        e.type = "ignored";
        e.detail = "capture_http_" + std::to_string(r.status);
        return e;
    }
    try {
        const auto& cap = j.at("purchase_units").at(0).at("payments").at("captures").at(0);
        e.reference = cap.value("id", "");
        e.orderId = cap.value("custom_id", e.orderId);
        e.currency = cap.at("amount").value("currency_code", "");
        e.amountMinor = decimalToMinor(cap.at("amount").value("value", ""), e.currency);
        std::string st = cap.value("status", "");
        e.type = st == "COMPLETED" ? "succeeded" : (st == "DECLINED" || st == "FAILED") ? "failed" : "ignored";
        if (e.type == "ignored") e.detail = "capture_status=" + st;
    } catch (const ApiError&) {
        throw;
    } catch (...) {
        throw ApiError(502, "provider_unavailable", "unexpected PayPal capture response");
    }
    return e;
}

Event paypalWebhook(Env&, const Provider& p, const httplib::Request& req) {
    json ev;
    try {
        ev = json::parse(req.body);
    } catch (...) {
        throw badRequest("invalid_json");
    }
    std::string token = paypalToken(p);
    json verify = {{"auth_algo", req.get_header_value("PAYPAL-AUTH-ALGO")},
                   {"cert_url", req.get_header_value("PAYPAL-CERT-URL")},
                   {"transmission_id", req.get_header_value("PAYPAL-TRANSMISSION-ID")},
                   {"transmission_sig", req.get_header_value("PAYPAL-TRANSMISSION-SIG")},
                   {"transmission_time", req.get_header_value("PAYPAL-TRANSMISSION-TIME")},
                   {"webhook_id", setting(p, "webhook_id")},
                   {"webhook_event", ev}};
    if (verify["webhook_id"].get<std::string>().empty()) throw ApiError(503, "provider_not_configured", "webhook_id");
    auto r = call("POST", paypalBase(p) + "/v1/notifications/verify-webhook-signature",
                  {{"Authorization", "Bearer " + token}}, verify.dump(), "application/json");
    json vr = parseJson(r);
    if (r.status != 200 || vr.value("verification_status", "") != "SUCCESS") throw unauthorized("invalid_signature");

    Event e;
    e.eventId = ev.value("id", "");
    std::string type = ev.value("event_type", "");
    const json res = ev.value("resource", json::object());
    if (type == "CHECKOUT.ORDER.APPROVED") {
        // The buyer approved: capture the payment now (server side).
        try {
            e.orderId = res.at("purchase_units").at(0).value("custom_id", "");
        } catch (...) {
        }
        return paypalCapture(p, token, res.value("id", ""), e);
    }
    if (type == "PAYMENT.CAPTURE.COMPLETED" || type == "PAYMENT.CAPTURE.DENIED" || type == "PAYMENT.CAPTURE.DECLINED") {
        e.orderId = res.value("custom_id", "");
        e.reference = res.value("id", "");
        if (res.contains("amount")) {
            e.currency = res["amount"].value("currency_code", "");
            e.amountMinor = decimalToMinor(res["amount"].value("value", ""), e.currency);
        }
        e.type = type == "PAYMENT.CAPTURE.COMPLETED" ? "succeeded" : "failed";
        return e;
    }
    e.type = "ignored";
    e.detail = type;
    return e;
}

}  // namespace

// ------------------------------------------------------------------ registry
bool Provider::supports(const std::string& currency) const {
    return currencies.empty() || std::find(currencies.begin(), currencies.end(), currency) != currencies.end();
}

json kinds() {
    auto f = [](const char* name, bool secret, bool required, const char* hint = "") {
        return json{{"name", name}, {"secret", secret}, {"required", required}, {"hint", hint}};
    };
    return json::array({
        {{"kind", "stripe"},
         {"label", "Stripe (cartes Visa, Mastercard…)"},
         {"implemented", true},
         {"fields", {f("secret_key", true, true, "sk_… / rk_… (clé restreinte conseillée)"),
                     f("webhook_secret", true, true, "whsec_…"), f("success_url", false, false),
                     f("cancel_url", false, false)}}},
        {{"kind", "paypal"},
         {"label", "PayPal"},
         {"implemented", true},
         {"fields", {f("client_id", false, true), f("client_secret", true, true), f("webhook_id", false, true),
                     f("return_url", false, false), f("cancel_url", false, false), f("brand_name", false, false)}}},
        {{"kind", "payoneer"},
         {"label", "Payoneer"},
         {"implemented", false},
         {"fields", {f("merchant_id", false, false), f("store_code", false, false), f("api_token", true, false)}}},
        {{"kind", "generic_hmac"},
         {"label", "Agrégateur (webhook signé Nhill)"},
         {"implemented", true},
         {"fields", {f("checkout_url_template", false, true, "https://… {order_id} {amount} {currency} {signature}"),
                     f("webhook_secret", true, true, "32 caractères minimum")}}},
    });
}

bool isKnownKind(const std::string& kind) {
    return kind == "stripe" || kind == "paypal" || kind == "payoneer" || kind == "generic_hmac";
}

namespace {
constexpr const char* kCols =
    "id, code, kind, display_name, environment, enabled, array_to_string(currencies, ','), settings, "
    "encode(secret_cipher, 'hex'), encode(secret_nonce, 'hex'), array_to_string(secret_fields, ',')";

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string x;
    while (std::getline(ss, x, ','))
        if (!x.empty()) out.push_back(x);
    return out;
}

Provider fromRow(Env& env, const pqxx::row& r, bool withSecrets) {
    Provider p;
    p.id = db::str(r[0]);
    p.code = db::str(r[1]);
    p.kind = db::str(r[2]);
    p.displayName = db::str(r[3]);
    p.environment = db::str(r[4]);
    p.enabled = db::boolean(r[5]);
    p.currencies = split(db::str(r[6]));
    p.settings = json::parse(db::str(r[7]));
    p.secretFields = split(db::str(r[10]));
    if (withSecrets && !r[8].is_null()) {
        auto c = crypto::fromHex(db::str(r[8]));
        auto n = crypto::fromHex(db::str(r[9]));
        auto plain = c && n ? crypto::open(env.cfg.providerKey, *c, *n) : std::nullopt;
        if (!plain) throw ApiError(500, "provider_secrets_unreadable", "provider secrets cannot be decrypted");
        p.secrets = json::parse(*plain);
    }
    return p;
}
}  // namespace

std::optional<Provider> byCode(Env& env, db::Tx& tx, const std::string& code, bool withSecrets) {
    auto r = tx.opt(std::string("SELECT ") + kCols + " FROM payment_providers WHERE code = $1", code);
    if (!r) return std::nullopt;
    return fromRow(env, *r, withSecrets);
}

std::optional<Provider> byId(Env& env, db::Tx& tx, const std::string& id, bool withSecrets) {
    auto r = tx.opt(std::string("SELECT ") + kCols + " FROM payment_providers WHERE id = $1", id);
    if (!r) return std::nullopt;
    return fromRow(env, *r, withSecrets);
}

json toJson(const Env& env, const Provider& p) {
    bool implemented = p.kind != "payoneer";
    return {{"id", p.id},
            {"code", p.code},
            {"kind", p.kind},
            {"display_name", p.displayName},
            {"environment", p.environment},
            {"enabled", p.enabled},
            {"implemented", implemented},
            {"currencies", p.currencies},
            {"settings", p.settings},
            {"secret_fields_set", p.secretFields},
            {"webhook_url", env.cfg.publicBaseUrl.empty() ? json(nullptr)
                                                          : json(env.cfg.publicBaseUrl + "/api/v1/webhooks/payments/" + p.code)}};
}

SealedSecrets sealSecrets(Env& env, const json& current, const json& updates) {
    json merged = current.is_object() ? current : json::object();
    for (auto it = updates.begin(); it != updates.end(); ++it) {
        if (!it.value().is_string()) throw invalid("secrets." + it.key(), "string_required");
        std::string v = it.value().get<std::string>();
        if (v.size() > 4096) throw invalid("secrets." + it.key(), "too_long");
        if (v.empty()) merged.erase(it.key());
        else merged[it.key()] = v;
    }
    SealedSecrets s;
    for (auto it = merged.begin(); it != merged.end(); ++it) s.fields.push_back(it.key());
    auto sealed = crypto::seal(env.cfg.providerKey, merged.dump());
    s.cipherHex = crypto::toHex(sealed.cipher);
    s.nonceHex = crypto::toHex(sealed.nonce);
    return s;
}

Checkout createCheckout(Env& env, const Provider& p, const OrderInfo& order) {
    if (!p.enabled) throw conflict("provider_disabled");
    if (!p.supports(order.currency)) throw conflict("provider_currency_unsupported");
    if (p.kind == "stripe") return stripeCheckout(env, p, order);
    if (p.kind == "paypal") return paypalCheckout(env, p, order);
    if (p.kind == "generic_hmac") return genericCheckout(env, p, order);
    throw ApiError(501, "provider_not_implemented", "no adapter for this provider yet");
}

Event parseWebhook(Env& env, const Provider& p, const httplib::Request& req) {
    if (p.kind == "stripe") return stripeWebhook(env, p, req);
    if (p.kind == "paypal") return paypalWebhook(env, p, req);
    if (p.kind == "generic_hmac") return genericWebhook(env, p, req);
    throw ApiError(501, "provider_not_implemented", "no adapter for this provider yet");
}

void setTransportForTests(Transport t) {
    std::lock_guard<std::mutex> l(gTransportMutex);
    gTransport = std::move(t);
}

}  // namespace nh::providers
