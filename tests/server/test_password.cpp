// Forgotten passwords (2.1.0): temporary password given by an establishment
// administrator (forced change at the next sign-in), one-time code e-mailed to
// administrators, SMTP delivery, and the security rules around them (scope of an
// administrator, no account enumeration, attempts, expiry, single use, sessions closed,
// nothing stored or logged in clear text).
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sodium.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <regex>
#include <thread>

#include "config.hpp"
#include "harness.hpp"
#include "util/crypto.hpp"
#include "util/mailer.hpp"

using namespace nht;

namespace {

const std::string kMemberPassword = "Membre-password-2026";

std::string memberId(const Tenant& t, const std::string& email) {
    auto r = Harness::get().get("/api/v1/members", t.token);
    for (const auto& m : r.body["members"])
        if (m["email"] == email) return m["id"];
    throw std::runtime_error("member not found: " + email);
}

Response login(const std::string& email, const std::string& password) {
    resetIpThrottles();
    return Harness::get().post("/api/v1/auth/login", {{"email", email}, {"password", password}});
}

// Per-account limits are tested separately: the other tests start from a clean counter.
void clearAccountThrottles(const std::string& email) {
    Harness::get().su("DELETE FROM auth_throttle WHERE bucket LIKE 'pwreset:admin:%'" +
                      (email.empty() ? std::string() : " OR bucket LIKE '%:" + email + "'"));
}

// The code of the last recovery e-mail queued for `email`, read like the mailer does (white box).
std::string queuedCode(const std::string& email) {
    auto& h = Harness::get();
    auto r = h.su("SELECT params->>'code_box', params->>'code_nonce' FROM email_outbox WHERE to_address = '" + email +
                  "' AND template = 'password_reset_code' ORDER BY created_at DESC LIMIT 1");
    if (r.empty() || r[0][0].is_null()) return "";
    auto code = nh::crypto::open(h.cfg().dataKey, *nh::crypto::fromHex(r[0][0].c_str()), *nh::crypto::fromHex(r[0][1].c_str()));
    return code.value_or("");
}

int count(const std::string& sql) { return Harness::get().su(sql)[0][0].as<int>(); }

// Minimal SMTP server on 127.0.0.1 (no TLS, no AUTH) recording the messages it receives.
class SmtpSink {
public:
    SmtpSink() {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        int one = 1;
        ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = 0;
        if (::bind(fd_, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || ::listen(fd_, 4) != 0)
            throw std::runtime_error("smtp sink: bind/listen failed");
        socklen_t len = sizeof a;
        ::getsockname(fd_, reinterpret_cast<sockaddr*>(&a), &len);
        port = ntohs(a.sin_port);
        thread_ = std::thread([this] { loop(); });
    }
    ~SmtpSink() {
        stop_ = true;
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        if (thread_.joinable()) thread_.join();
    }
    std::vector<std::string> messages() {
        std::lock_guard<std::mutex> l(mu_);
        return messages_;
    }
    std::vector<std::string> recipients() {
        std::lock_guard<std::mutex> l(mu_);
        return rcpts_;
    }
    int port = 0;

private:
    void loop() {
        while (!stop_) {
            int c = ::accept(fd_, nullptr, nullptr);
            if (c < 0) return;
            serve(c);
            ::close(c);
        }
    }
    static void say(int c, const std::string& s) { ::send(c, s.data(), s.size(), MSG_NOSIGNAL); }
    void serve(int c) {
        say(c, "220 sink ESMTP\r\n");
        std::string buf, data;
        bool inData = false;
        char tmp[4096];
        for (;;) {
            ssize_t n = ::recv(c, tmp, sizeof tmp, 0);
            if (n <= 0) return;
            buf.append(tmp, static_cast<size_t>(n));
            for (size_t eol; (eol = buf.find("\r\n")) != std::string::npos;) {
                std::string line = buf.substr(0, eol);
                buf.erase(0, eol + 2);
                if (inData) {
                    if (line == ".") {
                        inData = false;
                        {
                            std::lock_guard<std::mutex> l(mu_);
                            messages_.push_back(data);
                        }
                        data.clear();
                        say(c, "250 queued\r\n");
                    } else {
                        data += (line.rfind("..", 0) == 0 ? line.substr(1) : line) + "\r\n";
                    }
                    continue;
                }
                std::string verb = line.substr(0, 4);
                for (auto& ch : verb) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                if (verb == "EHLO") say(c, "250-sink\r\n250 8BITMIME\r\n");
                else if (verb == "HELO" || verb == "MAIL" || verb == "RSET" || verb == "NOOP") say(c, "250 OK\r\n");
                else if (verb == "RCPT") {
                    std::lock_guard<std::mutex> l(mu_);
                    rcpts_.push_back(line);
                    say(c, "250 OK\r\n");
                } else if (verb == "DATA") {
                    inData = true;
                    say(c, "354 go ahead\r\n");
                } else if (verb == "QUIT") {
                    say(c, "221 bye\r\n");
                    return;
                } else say(c, "502 not implemented\r\n");
            }
        }
    }
    int fd_ = -1;
    std::atomic<bool> stop_{false};
    std::thread thread_;
    std::mutex mu_;
    std::vector<std::string> messages_, rcpts_;
};

std::string decodeBody(const std::string& message) {
    auto p = message.find("\r\n\r\n");
    std::string b64 = p == std::string::npos ? "" : message.substr(p + 4);
    std::string out(b64.size(), '\0');
    size_t len = 0;
    if (sodium_base642bin(reinterpret_cast<unsigned char*>(out.data()), out.size(), b64.data(), b64.size(), "\r\n", &len,
                          nullptr, sodium_base64_VARIANT_ORIGINAL) != 0)
        return "";
    out.resize(len);
    return out;
}

// Sets the SMTP transport of the in-process server for the duration of a test.
struct SmtpTransport {
    explicit SmtpTransport(int port) : saved(Harness::get().cfg()) {
        auto& c = Harness::get().cfg();
        c.emailTransport = "smtp";
        c.smtpUrl = "smtp://127.0.0.1:" + std::to_string(port);
        c.smtpTls = false;
        c.smtpFrom = "Nhill Tests <no-reply@nhill.test>";
        c.smtpTimeoutSeconds = 5;
    }
    ~SmtpTransport() {
        auto& c = Harness::get().cfg();
        c.emailTransport = saved.emailTransport;
        c.smtpUrl = saved.smtpUrl;
        c.smtpTls = saved.smtpTls;
        c.smtpFrom = saved.smtpFrom;
        c.smtpTimeoutSeconds = saved.smtpTimeoutSeconds;
    }
    nh::Config saved;
};

void flushOutbox() {
    // Messages queued by other tests are delivered by the log transport first.
    auto& h = Harness::get();
    std::string t = h.cfg().emailTransport;
    h.cfg().emailTransport = "log";
    nh::jobs::dispatchEmails(h.env());
    h.cfg().emailTransport = t;
}

}  // namespace

// ------------------------------------------------------------------ administrator reset

TEST(PasswordAdminReset, TemporaryPasswordForcesANewPasswordAtNextSignIn) {
    auto& h = Harness::get();
    Tenant t = createTenant();
    std::string email;
    std::string memberToken = addMember(t, "sales", &email);
    std::string id = memberId(t, email);

    auto r = h.post("/api/v1/members/" + id + "/password-reset", json::object(), t.token);
    ASSERT_EQ(r.status, 200) << r.raw;
    const std::string temp = r.body["temporary_password"];
    EXPECT_TRUE(std::regex_match(temp, std::regex("^[a-km-np-zA-HJ-NP-Z2-9]{4}-[a-km-np-zA-HJ-NP-Z2-9]{4}-[a-km-np-zA-HJ-NP-Z2-9]{4}$")))
        << temp;
    EXPECT_GE(temp.size(), 8u);
    EXPECT_TRUE(r.body["must_change_password"].get<bool>());
    EXPECT_EQ(r.header("Cache-Control"), "no-store");

    // The member's sessions are closed and the old password no longer works.
    EXPECT_EQ(h.get("/api/v1/me", memberToken).status, 401);
    EXPECT_EQ(login(email, kMemberPassword).status, 401);

    // Nothing in clear text: not the hash, not the history, not the e-mail.
    EXPECT_EQ(count("SELECT count(*) FROM users WHERE email = '" + email + "' AND password_hash LIKE '%" + temp + "%'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM audit_log WHERE details::text LIKE '%" + temp + "%'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM email_outbox WHERE params::text LIKE '%" + temp + "%'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM idempotency_keys WHERE response_body::text LIKE '%" + temp + "%'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM audit_log WHERE action = 'member.password_reset' AND entity_id = '" + id + "'"), 1);
    EXPECT_EQ(count("SELECT count(*) FROM email_outbox WHERE to_address = '" + email + "' AND template = 'password_reset_by_admin'"), 1);

    // Signing in with the temporary password gives no session, only a change ticket.
    auto l = login(email, temp);
    ASSERT_EQ(l.status, 200) << l.raw;
    EXPECT_TRUE(l.body["password_change_required"].get<bool>());
    EXPECT_FALSE(l.body.contains("access_token"));
    const std::string ticket = l.body["change_token"];

    auto change = [&](const std::string& tk, const std::string& pw) {
        resetIpThrottles();
        return h.post("/api/v1/auth/password/change-required", {{"change_token", tk}, {"new_password", pw}});
    };
    EXPECT_EQ(change("not-a-ticket", "Nouveau-mot-de-passe-1").body["error"]["code"], "invalid_change_token");
    auto shortPw = change(ticket, "court");
    EXPECT_EQ(shortPw.status, 422);
    EXPECT_EQ(shortPw.body["error"]["details"]["reason"], "password_too_short");
    auto same = change(ticket, temp);
    EXPECT_EQ(same.status, 422);
    EXPECT_EQ(same.body["error"]["details"]["reason"], "password_reused");

    auto ok = change(ticket, "Nouveau-mot-de-passe-1");
    ASSERT_EQ(ok.status, 200) << ok.raw;
    ASSERT_TRUE(ok.body.contains("access_token"));
    EXPECT_EQ(h.get("/api/v1/me", ok.body["access_token"]).status, 200);
    EXPECT_EQ(count("SELECT count(*) FROM users WHERE email = '" + email + "' AND must_change_password"), 0);

    // The ticket is single use; the temporary password is gone; the new one works.
    EXPECT_EQ(change(ticket, "Encore-un-autre-mot-2").body["error"]["code"], "invalid_change_token");
    EXPECT_EQ(login(email, temp).status, 401);
    auto again = login(email, "Nouveau-mot-de-passe-1");
    EXPECT_EQ(again.status, 200);
    EXPECT_TRUE(again.body.contains("access_token"));
}

TEST(PasswordAdminReset, OnlyForMembersOfTheAdministratorsOwnEstablishment) {
    auto& h = Harness::get();
    Tenant a = createTenant();
    Tenant b = createTenant();
    std::string salesEmail, adminEmail, admin2Email;
    std::string salesToken = addMember(a, "sales", &salesEmail);
    std::string adminToken = addMember(a, "admin", &adminEmail);
    addMember(a, "admin", &admin2Email);
    const std::string salesId = memberId(a, salesEmail), adminId = memberId(a, adminEmail),
                      admin2Id = memberId(a, admin2Email), ownerId = memberId(a, a.ownerEmail);
    auto reset = [&](const std::string& member, const std::string& token) {
        clearAccountThrottles("");
        return h.post("/api/v1/members/" + member + "/password-reset", json::object(), token);
    };
    // A member without the team permission cannot reset anybody.
    EXPECT_EQ(reset(adminId, salesToken).status, 403);
    // Another establishment's owner cannot reach these members.
    EXPECT_EQ(reset(salesId, b.token).status, 404);
    // The owner is never reset by someone else; an administrator by the owner only; not oneself.
    EXPECT_EQ(reset(ownerId, adminToken).body["error"]["code"], "owner_immutable");
    EXPECT_EQ(reset(admin2Id, adminToken).body["error"]["code"], "owner_required");
    EXPECT_EQ(reset(adminId, adminToken).body["error"]["code"], "cannot_modify_self");
    EXPECT_EQ(reset(ownerId, a.token).body["error"]["code"], "cannot_modify_self");
    // An administrator can reset a sales member; the owner can reset an administrator.
    EXPECT_EQ(reset(salesId, adminToken).status, 200);
    EXPECT_EQ(reset(admin2Id, a.token).status, 200);
    // A disabled access must be re-enabled first.
    ASSERT_EQ(h.request("PATCH", "/api/v1/members/" + salesId, {{"status", "disabled"}}, a.token).status, 204);
    EXPECT_EQ(reset(salesId, a.token).body["error"]["code"], "member_disabled");
    // Nothing was written for refused attempts.
    EXPECT_EQ(count("SELECT count(*) FROM audit_log WHERE action = 'member.password_reset' AND entity_id = '" + ownerId + "'"), 0);
}

TEST(PasswordAdminReset, AccountUsedInAnotherEstablishmentIsNotResetByOneOfThem) {
    auto& h = Harness::get();
    Tenant a = createTenant();
    Tenant b = createTenant();
    std::string email;
    addMember(a, "sales", &email);
    // The same person joins establishment B with the same account.
    resetIpThrottles();
    auto inv = h.post("/api/v1/invitations", {{"email", email}, {"role", "viewer"}}, b.token);
    ASSERT_EQ(inv.status, 201) << inv.raw;
    auto chk = h.post("/api/v1/join/check-code", {{"code", b.code}});
    auto act = h.post("/api/v1/join/activate", {{"join_ticket", chk.body["join_ticket"]}, {"email", email},
                                                {"activation_code", inv.body["activation_code"]}, {"password", kMemberPassword}});
    ASSERT_EQ(act.status, 200) << act.raw;
    auto r = h.post("/api/v1/members/" + memberId(a, email) + "/password-reset", json::object(), a.token);
    EXPECT_EQ(r.status, 409);
    EXPECT_EQ(r.body["error"]["code"], "account_shared");
    EXPECT_EQ(login(email, kMemberPassword).status, 200);  // unchanged
}

TEST(PasswordAdminReset, ExpiredTemporaryPasswordIsRefused) {
    auto& h = Harness::get();
    Tenant t = createTenant();
    std::string email;
    addMember(t, "viewer", &email);
    auto r = h.post("/api/v1/members/" + memberId(t, email) + "/password-reset", json::object(), t.token);
    ASSERT_EQ(r.status, 200);
    h.su("UPDATE users SET temporary_password_expires_at = now() - interval '1 minute' WHERE email = '" + email + "'");
    auto l = login(email, r.body["temporary_password"]);
    EXPECT_EQ(l.status, 401);
    EXPECT_EQ(l.body["error"]["code"], "temporary_password_expired");
    // A wrong password still gets the generic answer.
    EXPECT_EQ(login(email, "Pas-le-bon-mot-de-passe").body["error"]["code"], "invalid_credentials");
}

TEST(PasswordAdminReset, TemporaryPasswordCannotBeUsedToJoinAnotherEstablishment) {
    auto& h = Harness::get();
    Tenant a = createTenant();
    Tenant b = createTenant();
    std::string email;
    addMember(a, "sales", &email);
    auto r = h.post("/api/v1/members/" + memberId(a, email) + "/password-reset", json::object(), a.token);
    ASSERT_EQ(r.status, 200);
    resetIpThrottles();
    auto inv = h.post("/api/v1/invitations", {{"email", email}, {"role", "viewer"}}, b.token);
    auto chk = h.post("/api/v1/join/check-code", {{"code", b.code}});
    auto act = h.post("/api/v1/join/activate", {{"join_ticket", chk.body["join_ticket"]}, {"email", email},
                                                {"activation_code", inv.body["activation_code"]},
                                                {"password", r.body["temporary_password"]}});
    EXPECT_EQ(act.status, 409);
    EXPECT_EQ(act.body["error"]["code"], "password_change_required");
}

// ------------------------------------------------------------------ e-mail recovery

TEST(PasswordEmailRecovery, SameAnswerWhateverTheAccount) {
    auto& h = Harness::get();
    Tenant t = createTenant();
    std::string memberEmail;
    addMember(t, "sales", &memberEmail);
    auto forgot = [&](const std::string& email) {
        resetIpThrottles();
        return h.post("/api/v1/auth/password/forgot", {{"email", email}});
    };
    auto unknown = forgot(uniq("nobody") + "@example.cd");
    auto member = forgot(memberEmail);
    auto owner = forgot(t.ownerEmail);
    for (const auto* r : {&unknown, &member, &owner}) {
        EXPECT_EQ(r->status, 202) << r->raw;
        EXPECT_EQ(r->raw, unknown.raw);
    }
    // Owner: a code; member of one establishment: an e-mail pointing to the administrator.
    EXPECT_EQ(count("SELECT count(*) FROM password_resets p JOIN users u ON u.id = p.user_id WHERE u.email = '" +
                    t.ownerEmail + "' AND purpose = 'email_code'"),
              1);
    EXPECT_EQ(count("SELECT count(*) FROM password_resets p JOIN users u ON u.id = p.user_id WHERE u.email = '" +
                    memberEmail + "'"),
              0);
    EXPECT_EQ(count("SELECT count(*) FROM email_outbox WHERE to_address = '" + memberEmail +
                    "' AND template = 'password_reset_contact_admin'"),
              1);
    // The code exists only sealed in the outbox and as an HMAC in password_resets.
    std::string code = queuedCode(t.ownerEmail);
    ASSERT_EQ(code.size(), 8u);
    EXPECT_EQ(count("SELECT count(*) FROM email_outbox WHERE params::text LIKE '%" + code + "%'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM password_resets WHERE encode(secret_hash,'escape') LIKE '%" + code + "%'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM audit_log WHERE details::text LIKE '%" + code + "%'"), 0);
    // Malformed address: validation error, not an account hint.
    EXPECT_EQ(forgot("not-an-email").status, 422);
}

TEST(PasswordEmailRecovery, CodeResetsThePasswordOnceAndClosesSessions) {
    auto& h = Harness::get();
    Tenant t = createTenant();
    resetIpThrottles();
    ASSERT_EQ(h.post("/api/v1/auth/password/forgot", {{"email", t.ownerEmail}}).status, 202);
    std::string code = queuedCode(t.ownerEmail);
    ASSERT_EQ(code.size(), 8u);
    auto reset = [&](const std::string& c, const std::string& pw) {
        resetIpThrottles();
        return h.post("/api/v1/auth/password/reset", {{"email", t.ownerEmail}, {"code", c}, {"new_password", pw}});
    };
    std::string wrong = code == "00000000" ? "11111111" : "00000000";
    auto bad = reset(wrong, "Proprietaire-nouveau-1");
    EXPECT_EQ(bad.status, 400);
    EXPECT_EQ(bad.body["error"]["code"], "invalid_reset_code");
    EXPECT_EQ(count("SELECT attempts FROM password_resets p JOIN users u ON u.id = p.user_id WHERE u.email = '" +
                    t.ownerEmail + "'"),
              1);
    EXPECT_EQ(reset(code, "court").body["error"]["details"]["reason"], "password_too_short");

    auto ok = reset(code.substr(0, 4) + "-" + code.substr(4), "Proprietaire-nouveau-1");
    EXPECT_EQ(ok.status, 204) << ok.raw;
    // Existing sessions closed, old password refused, code single use.
    EXPECT_EQ(h.get("/api/v1/me", t.token).status, 401);
    EXPECT_EQ(login(t.ownerEmail, t.password).status, 401);
    EXPECT_EQ(login(t.ownerEmail, "Proprietaire-nouveau-1").status, 200);
    EXPECT_EQ(reset(code, "Proprietaire-encore-2").body["error"]["code"], "invalid_reset_code");
    EXPECT_EQ(count("SELECT count(*) FROM email_outbox WHERE to_address = '" + t.ownerEmail +
                    "' AND template = 'password_changed'"),
              1);
    EXPECT_EQ(count("SELECT count(*) FROM audit_log a JOIN users u ON a.entity_id = u.id::text WHERE u.email = '" +
                    t.ownerEmail + "' AND a.action = 'auth.password_reset'"),
              1);
}

TEST(PasswordEmailRecovery, AttemptsExpiryAndNewRequestInvalidateTheCode) {
    auto& h = Harness::get();
    Tenant t = createTenant();
    auto forgot = [&] {
        resetIpThrottles();
        clearAccountThrottles(t.ownerEmail);
        return h.post("/api/v1/auth/password/forgot", {{"email", t.ownerEmail}});
    };
    auto reset = [&](const std::string& c) {
        resetIpThrottles();
        return h.post("/api/v1/auth/password/reset",
                      {{"email", t.ownerEmail}, {"code", c}, {"new_password", "Proprietaire-nouveau-1"}});
    };
    // 5 wrong codes: the code is revoked, the right one no longer works.
    forgot();
    std::string code = queuedCode(t.ownerEmail);
    std::string wrong = code == "00000000" ? "11111111" : "00000000";
    for (int i = 0; i < 5; ++i) EXPECT_EQ(reset(wrong).status, 400);
    EXPECT_EQ(reset(code).body["error"]["code"], "invalid_reset_code");
    // Expired code.
    forgot();
    code = queuedCode(t.ownerEmail);
    h.su("UPDATE password_resets p SET expires_at = now() - interval '1 second' FROM users u WHERE u.id = p.user_id "
         "AND u.email = '" + t.ownerEmail + "' AND p.used_at IS NULL AND p.revoked_at IS NULL");
    EXPECT_EQ(reset(code).body["error"]["code"], "invalid_reset_code");
    // A new request revokes the previous code.
    forgot();
    std::string first = queuedCode(t.ownerEmail);
    forgot();
    std::string second = queuedCode(t.ownerEmail);
    if (first != second) {
        EXPECT_EQ(reset(first).body["error"]["code"], "invalid_reset_code");
    }
    EXPECT_EQ(reset(second).status, 204);
}

TEST(PasswordEmailRecovery, RepeatedRequestsAreThrottledIdenticallyForUnknownAddresses) {
    auto& h = Harness::get();
    Tenant t = createTenant();
    std::string unknown = uniq("nobody") + "@example.cd";
    for (const std::string& email : {t.ownerEmail, unknown}) {
        std::vector<int> statuses;
        for (int i = 0; i < 4; ++i) {
            resetIpThrottles();
            statuses.push_back(h.post("/api/v1/auth/password/forgot", {{"email", email}}).status);
        }
        EXPECT_EQ(statuses, (std::vector<int>{202, 202, 202, 429})) << email;
    }
    // The per-address limit of code checks.
    std::vector<int> statuses;
    for (int i = 0; i < 11; ++i) {
        resetIpThrottles();
        statuses.push_back(h.post("/api/v1/auth/password/reset", {{"email", unknown}, {"code", "12345678"},
                                                                  {"new_password", "Proprietaire-nouveau-1"}})
                               .status);
    }
    EXPECT_EQ(statuses.back(), 429);
    EXPECT_EQ(statuses.front(), 400);
}

TEST(PasswordEmailRecovery, AccountSharedByTwoEstablishmentsReceivesACode) {
    auto& h = Harness::get();
    Tenant a = createTenant();
    Tenant b = createTenant();
    std::string email;
    addMember(a, "sales", &email);
    resetIpThrottles();
    auto inv = h.post("/api/v1/invitations", {{"email", email}, {"role", "viewer"}}, b.token);
    auto chk = h.post("/api/v1/join/check-code", {{"code", b.code}});
    ASSERT_EQ(h.post("/api/v1/join/activate", {{"join_ticket", chk.body["join_ticket"]}, {"email", email},
                                               {"activation_code", inv.body["activation_code"]}, {"password", kMemberPassword}})
                  .status,
              200);
    resetIpThrottles();
    ASSERT_EQ(h.post("/api/v1/auth/password/forgot", {{"email", email}}).status, 202);
    EXPECT_EQ(queuedCode(email).size(), 8u);
}

// ------------------------------------------------------------------ delivery

TEST(PasswordEmailDelivery, CodeIsDeliveredBySmtpThenErasedFromTheOutbox) {
    auto& h = Harness::get();
    flushOutbox();
    Tenant t = createTenant();
    flushOutbox();
    SmtpSink sink;
    SmtpTransport smtp(sink.port);
    resetIpThrottles();
    ASSERT_EQ(h.post("/api/v1/auth/password/forgot", {{"email", t.ownerEmail}}).status, 202);
    auto st = nh::jobs::dispatchEmails(h.env());
    EXPECT_EQ(st.emailsSent, 1);
    EXPECT_EQ(st.emailFailures, 0);
    auto msgs = sink.messages();
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_NE(msgs[0].find("Content-Type: text/plain; charset=UTF-8"), std::string::npos);
    EXPECT_NE(msgs[0].find("Subject: =?UTF-8?B?"), std::string::npos);
    ASSERT_EQ(sink.recipients().size(), 1u);
    EXPECT_NE(sink.recipients()[0].find("<" + t.ownerEmail + ">"), std::string::npos);
    std::string body = decodeBody(msgs[0]);
    std::smatch m;
    ASSERT_TRUE(std::regex_search(body, m, std::regex("([0-9]{4})-([0-9]{4})"))) << body;
    EXPECT_NE(body.find("20 minutes"), std::string::npos) << body;  // French template (owner's language)
    // Sent: the sealed code is erased from the outbox.
    EXPECT_EQ(count("SELECT count(*) FROM email_outbox WHERE to_address = '" + t.ownerEmail +
                    "' AND template = 'password_reset_code' AND status = 'sent' AND params = '{}'::jsonb"),
              1);
    resetIpThrottles();
    auto r = h.post("/api/v1/auth/password/reset",
                    {{"email", t.ownerEmail}, {"code", m[1].str() + m[2].str()}, {"new_password", "Recu-par-email-2026"}});
    EXPECT_EQ(r.status, 204) << r.raw;
    EXPECT_EQ(login(t.ownerEmail, "Recu-par-email-2026").status, 200);
}

TEST(PasswordEmailDelivery, FailedDeliveryIsRetriedThenAbandonedWithoutKeepingTheCode) {
    auto& h = Harness::get();
    flushOutbox();
    Tenant t = createTenant();
    flushOutbox();
    int closedPort;
    {
        SmtpSink tmp;
        closedPort = tmp.port;
    }  // nothing listens there any more
    SmtpTransport smtp(closedPort);
    resetIpThrottles();
    ASSERT_EQ(h.post("/api/v1/auth/password/forgot", {{"email", t.ownerEmail}}).status, 202);
    for (int i = 1; i <= 5; ++i) {
        auto st = nh::jobs::dispatchEmails(h.env());
        EXPECT_EQ(st.emailFailures, 1);
        auto row = h.su("SELECT status, attempts, last_error, params ? 'code_box' FROM email_outbox WHERE to_address = '" +
                        t.ownerEmail + "' AND template = 'password_reset_code'");
        ASSERT_EQ(row.size(), 1u);
        EXPECT_EQ(row[0][1].as<int>(), i);
        EXPECT_FALSE(row[0][2].is_null());
        EXPECT_EQ(std::string(row[0][0].c_str()), i < 5 ? "pending" : "failed");
        EXPECT_EQ(row[0][3].as<bool>(), i < 5);
    }
    EXPECT_EQ(nh::jobs::dispatchEmails(h.env()).emailFailures, 0);  // abandoned
}

TEST(PasswordEmailDelivery, ExpiredCodeIsNotSent) {
    auto& h = Harness::get();
    flushOutbox();
    Tenant t = createTenant();
    flushOutbox();
    SmtpSink sink;
    SmtpTransport smtp(sink.port);
    resetIpThrottles();
    ASSERT_EQ(h.post("/api/v1/auth/password/forgot", {{"email", t.ownerEmail}}).status, 202);
    h.su("UPDATE email_outbox SET params = jsonb_set(params, '{expires_at}', to_jsonb((now() - interval '1 minute')::text)) "
         "WHERE to_address = '" + t.ownerEmail + "' AND template = 'password_reset_code'");
    nh::jobs::dispatchEmails(h.env());
    EXPECT_TRUE(sink.messages().empty());
    EXPECT_EQ(count("SELECT count(*) FROM email_outbox WHERE to_address = '" + t.ownerEmail +
                    "' AND status = 'failed' AND last_error = 'expired' AND params = '{}'::jsonb"),
              1);
}

TEST(PasswordEmailDelivery, HeadersCannotBeInjected) {
    nh::mail::Message m{"a@example.cd", "Objet\r\nBcc: victim@example.cd", "Ligne 1\nLigne 2"};
    std::string mime = nh::mail::buildMime("Nhill <no-reply@example.cd>", m, "Tue, 27 Oct 2026 08:00:00 +0000", "id@example.cd");
    EXPECT_EQ(mime.find("\r\nBcc:"), std::string::npos);
    EXPECT_NE(mime.find("From: Nhill <no-reply@example.cd>\r\n"), std::string::npos);
    EXPECT_EQ(nh::mail::addressOf("Équipe Nhill <no-reply@example.cd>"), "no-reply@example.cd");
    std::string accented = nh::mail::buildMime("Équipe Nhill <no-reply@example.cd>", m, "d", "i@x");
    EXPECT_NE(accented.find("From: =?UTF-8?B?"), std::string::npos);
}

TEST(PasswordEmailDelivery, TemplatesAreTranslatedAndFilled) {
    auto& h = Harness::get();
    auto fr = nh::jobs::renderEmail(h.env(), "a@example.cd", "fr", "password_reset_contact_admin", {{"establishment", "Boutique Lumière"}});
    EXPECT_NE(fr.body.find("« Boutique Lumière »"), std::string::npos);
    EXPECT_EQ(fr.body.find('{'), std::string::npos);
    auto en = nh::jobs::renderEmail(h.env(), "a@example.cd", "en", "subscription_reminder",
                                    {{"establishment", "Shop"}, {"period_end", "2026-11-30T10:00:00Z"}});
    EXPECT_NE(en.body.find("expires on 2026-11-30."), std::string::npos) << en.body;
    for (const char* tmpl : {"code_rotated", "subscription_reminder", "subscription_grace", "subscription_suspended",
                             "password_reset_code", "password_reset_contact_admin", "password_reset_by_admin", "password_changed"})
        for (const char* lang : {"fr", "en"}) {
            EXPECT_NE(h.env().i18n.t(lang, std::string("email.") + tmpl + ".subject").rfind("email.", 0), 0u) << tmpl << lang;
            EXPECT_NE(h.env().i18n.t(lang, std::string("email.") + tmpl + ".body").rfind("email.", 0), 0u) << tmpl << lang;
        }
}

TEST(PasswordEmailDelivery, SignatureGivesTheSupportAddressOnlyWhenConfigured) {
    auto& h = Harness::get();
    const std::string saved = h.cfg().supportEmail;
    h.cfg().supportEmail.clear();
    auto plain = nh::jobs::renderEmail(h.env(), "a@example.com", "fr", "password_changed", json::object());
    EXPECT_NE(plain.body.find("ne répondez pas"), std::string::npos) << plain.body;
    EXPECT_EQ(plain.body.find('@'), std::string::npos) << plain.body;
    h.cfg().supportEmail = "support@nhill.online";
    for (const char* lang : {"fr", "en"}) {
        auto m = nh::jobs::renderEmail(h.env(), "a@example.com", lang, "password_changed", json::object());
        EXPECT_NE(m.body.find("support@nhill.online."), std::string::npos) << lang << m.body;
        EXPECT_EQ(m.body.find('{'), std::string::npos) << lang << m.body;
    }
    h.cfg().supportEmail = saved;
}

// ------------------------------------------------------------------ configuration

TEST(PasswordEmailConfig, SmtpRequiresTlsExceptOnLoopbackAndNoCredentialsInTheUrl) {
    ::setenv("NH_SECRET_KEY", "8f3a0c5e7b9d1f2a4c6e8b0d2f4a6c8e0b2d4f6a8c0e2b4d6f8a0c2e4b6d8f0a", 1);
    ::setenv("NH_EMAIL_TRANSPORT", "smtp", 1);
    ::setenv("NH_SMTP_FROM", "Nhill <no-reply@example.cd>", 1);
    auto load = [](const char* url, const char* tls) {
        ::setenv("NH_SMTP_URL", url, 1);
        ::setenv("NH_SMTP_TLS", tls, 1);
        return nh::Config::load(std::nullopt);
    };
    EXPECT_NO_THROW(load("smtps://smtp.example.cd:465", "required"));
    EXPECT_NO_THROW(load("smtp://smtp.example.cd:587", "required"));
    EXPECT_NO_THROW(load("smtp://127.0.0.1:25", "off"));
    EXPECT_THROW(load("smtp://smtp.example.cd:25", "off"), std::runtime_error);
    EXPECT_THROW(load("smtps://user:pw@smtp.example.cd:465", "required"), std::runtime_error);
    EXPECT_THROW(load("http://smtp.example.cd", "required"), std::runtime_error);
    ::setenv("NH_SMTP_USERNAME", "nhill", 1);
    EXPECT_THROW(load("smtps://smtp.example.cd:465", "required"), std::runtime_error);  // password missing
    ::setenv("NH_SMTP_PASSWORD", "secret-smtp-password", 1);
    try {
        auto c = load("smtps://smtp.example.cd:465", "required");
        EXPECT_EQ(c.smtpPassword, "secret-smtp-password");
    } catch (const std::exception& e) {
        ADD_FAILURE() << e.what();
    }
    ::setenv("NH_EMAIL_TRANSPORT", "carrier-pigeon", 1);
    EXPECT_THROW(nh::Config::load(std::nullopt), std::runtime_error);
    for (const char* k : {"NH_SECRET_KEY", "NH_EMAIL_TRANSPORT", "NH_SMTP_FROM", "NH_SMTP_URL", "NH_SMTP_TLS",
                          "NH_SMTP_USERNAME", "NH_SMTP_PASSWORD"})
        ::unsetenv(k);
}

TEST(PasswordEmailConfig, SupportAddressIsOneValidAddress) {
    ::setenv("NH_SECRET_KEY", "8f3a0c5e7b9d1f2a4c6e8b0d2f4a6c8e0b2d4f6a8c0e2b4d6f8a0c2e4b6d8f0a", 1);
    ::setenv("NH_EMAIL_TRANSPORT", "log", 1);
    ::setenv("NH_SUPPORT_EMAIL", "support@nhill.online", 1);
    EXPECT_EQ(nh::Config::load(std::nullopt).supportEmail, "support@nhill.online");
    for (const char* bad : {"support", "@nhill.online", "a@b, c@d", "Support <support@nhill.online>", "a@b\r\nBcc: x@y"}) {
        ::setenv("NH_SUPPORT_EMAIL", bad, 1);
        EXPECT_THROW(nh::Config::load(std::nullopt), std::runtime_error) << bad;
    }
    ::unsetenv("NH_SUPPORT_EMAIL");
    EXPECT_TRUE(nh::Config::load(std::nullopt).supportEmail.empty());
    ::unsetenv("NH_EMAIL_TRANSPORT");
    ::unsetenv("NH_SECRET_KEY");
}
