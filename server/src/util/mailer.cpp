#include "util/mailer.hpp"

#include <curl/curl.h>
#include <sodium.h>

#include <cstring>
#include <ctime>
#include <mutex>
#include <stdexcept>

#include "config.hpp"
#include "util/crypto.hpp"

namespace nh::mail {

namespace {

std::string base64(const std::string& in) {
    if (in.empty()) return {};
    std::string out(sodium_base64_ENCODED_LEN(in.size(), sodium_base64_VARIANT_ORIGINAL), '\0');
    sodium_bin2base64(out.data(), out.size(), reinterpret_cast<const unsigned char*>(in.data()), in.size(),
                      sodium_base64_VARIANT_ORIGINAL);
    out.resize(std::strlen(out.c_str()));
    return out;
}

// Header values come from templates and the database: no line break may reach the
// header block (header injection).
std::string oneLine(std::string s) {
    for (auto& c : s)
        if (c == '\r' || c == '\n') c = ' ';
    return s;
}

bool isAscii(const std::string& s) {
    for (unsigned char c : s)
        if (c >= 0x80) return false;
    return true;
}

// RFC 2047 encoded-word (UTF-8, base64), split so that each word stays under 75 chars.
std::string encodeWord(const std::string& s) {
    if (isAscii(s)) return s;
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        size_t n = std::min<size_t>(45, s.size() - i);
        // Never cut a UTF-8 sequence in two.
        while (i + n < s.size() && (static_cast<unsigned char>(s[i + n]) & 0xC0) == 0x80) --n;
        if (!out.empty()) out += "\r\n ";
        out += "=?UTF-8?B?" + base64(s.substr(i, n)) + "?=";
        i += n;
    }
    return out;
}

std::string encodeFrom(const std::string& from) {
    auto lt = from.find('<');
    if (lt == std::string::npos) return from;
    std::string name = from.substr(0, lt);
    while (!name.empty() && name.back() == ' ') name.pop_back();
    if (name.empty()) return from.substr(lt);
    return encodeWord(name) + " " + from.substr(lt);
}

struct Upload {
    const std::string* data;
    size_t pos = 0;
};

size_t readCb(char* buffer, size_t size, size_t nitems, void* userp) {
    auto* u = static_cast<Upload*>(userp);
    size_t room = size * nitems;
    size_t left = u->data->size() - u->pos;
    size_t n = std::min(room, left);
    std::memcpy(buffer, u->data->data() + u->pos, n);
    u->pos += n;
    return n;
}

std::once_flag gCurlInit;

}  // namespace

std::string addressOf(const std::string& header) {
    auto lt = header.find('<');
    if (lt == std::string::npos) return header;
    auto gt = header.find('>', lt);
    return header.substr(lt + 1, gt == std::string::npos ? std::string::npos : gt - lt - 1);
}

std::string buildMime(const std::string& from, const Message& m, const std::string& dateHeader,
                      const std::string& messageId) {
    std::string b64 = base64(m.body);
    std::string wrapped;
    for (size_t i = 0; i < b64.size(); i += 76) wrapped += b64.substr(i, 76) + "\r\n";
    return "Date: " + oneLine(dateHeader) + "\r\n" +
           "From: " + encodeFrom(oneLine(from)) + "\r\n" +
           "To: <" + oneLine(m.to) + ">\r\n" +
           "Subject: " + encodeWord(oneLine(m.subject)) + "\r\n" +
           "Message-ID: <" + oneLine(messageId) + ">\r\n" +
           "MIME-Version: 1.0\r\n"
           "Content-Type: text/plain; charset=UTF-8\r\n"
           "Content-Transfer-Encoding: base64\r\n"
           "Auto-Submitted: auto-generated\r\n"
           "\r\n" +
           wrapped;
}

void send(const Config& cfg, const Message& m) {
    std::call_once(gCurlInit, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    if (cfg.smtpUrl.empty()) throw std::runtime_error("SMTP relay not configured (NH_SMTP_URL)");
    if (m.to.find_first_of("\r\n<> ") != std::string::npos || m.to.find('@') == std::string::npos)
        throw std::runtime_error("invalid recipient address");

    char date[64];
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&now, &tm);
    std::strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S +0000", &tm);
    const std::string fromAddr = addressOf(cfg.smtpFrom);
    const std::string domain = fromAddr.substr(fromAddr.find('@') + 1);
    const std::string payload = buildMime(cfg.smtpFrom, m, date, crypto::toHex(crypto::randomBytes(16)) + "@" + domain);

    CURL* c = curl_easy_init();
    if (!c) throw std::runtime_error("libcurl initialisation failed");
    curl_slist* rcpt = curl_slist_append(nullptr, ("<" + m.to + ">").c_str());
    Upload up{&payload};
    char errbuf[CURL_ERROR_SIZE] = {0};
    const std::string mailFrom = "<" + fromAddr + ">";
    curl_easy_setopt(c, CURLOPT_URL, cfg.smtpUrl.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "smtp,smtps");
#else
    curl_easy_setopt(c, CURLOPT_PROTOCOLS, CURLPROTO_SMTP | CURLPROTO_SMTPS);
#endif
    curl_easy_setopt(c, CURLOPT_USE_SSL, cfg.smtpTls ? CURLUSESSL_ALL : CURLUSESSL_NONE);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
    if (!cfg.smtpCaFile.empty()) curl_easy_setopt(c, CURLOPT_CAINFO, cfg.smtpCaFile.c_str());
    if (!cfg.smtpUsername.empty()) {
        curl_easy_setopt(c, CURLOPT_USERNAME, cfg.smtpUsername.c_str());
        curl_easy_setopt(c, CURLOPT_PASSWORD, cfg.smtpPassword.c_str());
    }
    curl_easy_setopt(c, CURLOPT_MAIL_FROM, mailFrom.c_str());
    curl_easy_setopt(c, CURLOPT_MAIL_RCPT, rcpt);
    curl_easy_setopt(c, CURLOPT_READFUNCTION, readCb);
    curl_easy_setopt(c, CURLOPT_READDATA, &up);
    curl_easy_setopt(c, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);  // thread safety (no SIGALRM)
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, static_cast<long>(cfg.smtpTimeoutSeconds));
    curl_easy_setopt(c, CURLOPT_TIMEOUT, static_cast<long>(cfg.smtpTimeoutSeconds * 3));
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);
    CURLcode rc = curl_easy_perform(c);
    long smtpCode = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &smtpCode);
    curl_slist_free_all(rcpt);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) {
        // libcurl's messages describe the transport (host, TLS, SMTP code), never the credentials.
        std::string detail = errbuf[0] ? errbuf : curl_easy_strerror(rc);
        throw std::runtime_error("SMTP delivery failed (" + std::string(curl_easy_strerror(rc)) + ", SMTP " +
                                 std::to_string(smtpCode) + "): " + oneLine(detail));
    }
}

}  // namespace nh::mail
