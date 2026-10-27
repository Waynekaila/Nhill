// Outgoing e-mail through an SMTP relay (libcurl). TLS is required (implicit TLS with
// smtps:// or STARTTLS with smtp://) unless the relay is on this machine and
// NH_SMTP_TLS=off. Error messages never contain the SMTP password nor the body.
#pragma once

#include <string>

namespace nh {
struct Config;
}

namespace nh::mail {

struct Message {
    std::string to;       // single recipient address (already validated)
    std::string subject;  // UTF-8, one line
    std::string body;     // UTF-8 plain text, '\n' line ends
};

// "Nhill <no-reply@example.com>" -> "no-reply@example.com"
std::string addressOf(const std::string& header);

// RFC 5322 message (headers + base64 body). Exposed for tests.
std::string buildMime(const std::string& from, const Message& m, const std::string& dateHeader,
                      const std::string& messageId);

// Sends one message; throws std::runtime_error (secret-free text) on failure.
void send(const Config& cfg, const Message& m);

}  // namespace nh::mail
