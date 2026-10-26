// Payment providers configured from Nhill Console.
//
// Each provider row (payment_providers) holds non-secret settings in clear and its
// secrets sealed with a key derived from NH_SECRET_KEY (never stored in the
// database, never sent to any client). Adapters translate between Nhill orders
// and each provider's hosted checkout and webhook format:
//
//   generic_hmac : Nhill's own signed webhook (X-Nhill-Timestamp / X-Nhill-Signature),
//                  for a payment aggregator or bank that can call it;
//   stripe       : Checkout Sessions (card payments, Visa / Mastercard...), webhook
//                  "Stripe-Signature", session re-read from the API before applying;
//   paypal       : Orders v2 (create + capture), webhook verified by PayPal's
//                  verify-webhook-signature API;
//   payoneer     : configuration only — no adapter yet (API documentation and test
//                  credentials needed); checkout and webhooks answer 501.
//
// Adapters are exercised by automated tests against a local fake transport only:
// they have not been verified against the real provider APIs.
#pragma once

#include <httplib.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "services/common.hpp"

namespace nh::providers {

struct Provider {
    std::string id, code, kind, displayName, environment;
    bool enabled = false;
    std::vector<std::string> currencies;   // empty = any
    json settings = json::object();
    json secrets = json::object();         // decrypted only when requested
    std::vector<std::string> secretFields;
    bool supports(const std::string& currency) const;
};

// Field specification per kind (used by the console form and for validation).
json kinds();
bool isKnownKind(const std::string& kind);

std::optional<Provider> byCode(Env& env, db::Tx& tx, const std::string& code, bool withSecrets);
std::optional<Provider> byId(Env& env, db::Tx& tx, const std::string& id, bool withSecrets);
json toJson(const Env& env, const Provider& p);   // never contains secrets

// Merges new secret values into the existing ones (empty string = remove) and seals them.
struct SealedSecrets {
    std::string cipherHex, nonceHex;
    std::vector<std::string> fields;
};
SealedSecrets sealSecrets(Env& env, const json& current, const json& updates);

struct OrderInfo {
    std::string id;
    int64_t amountMinor = 0;
    std::string currency;
    std::string description;
    std::string establishmentName;
    std::string customerEmail;
};
struct Checkout {
    std::string session;   // provider-side id
    std::string url;       // hosted payment page
};
Checkout createCheckout(Env& env, const Provider& p, const OrderInfo& order);

struct Event {
    std::string eventId;
    std::string type;       // succeeded | failed | refunded | ignored
    std::string orderId;
    std::string reference;  // provider payment reference
    int64_t amountMinor = 0;
    std::string currency;
    std::string detail;
};
// Verifies the request (signature or provider API) and normalises it. Throws 401 on
// an unverifiable request.
Event parseWebhook(Env& env, const Provider& p, const httplib::Request& req);

// ---- outbound HTTP (replaceable by tests)
struct HttpResponse {
    int status = 0;
    std::string body;
};
using Transport = std::function<HttpResponse(const std::string& method, const std::string& url,
                                             const httplib::Headers& headers, const std::string& body,
                                             const std::string& contentType)>;
void setTransportForTests(Transport t);   // nullptr restores the real HTTPS client

}  // namespace nh::providers
