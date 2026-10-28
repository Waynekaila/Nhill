#include "util/zip.hpp"

#include <zlib.h>

#include <limits>
#include <stdexcept>

namespace nh::zip {

namespace {

void le16(std::string& o, uint16_t v) {
    o.push_back(static_cast<char>(v & 0xff));
    o.push_back(static_cast<char>((v >> 8) & 0xff));
}
void le32(std::string& o, uint32_t v) {
    for (int i = 0; i < 4; ++i) o.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
}
uint16_t rd16(const std::string& s, size_t p) {
    if (p + 2 > s.size()) throw std::runtime_error("zip: truncated");
    return static_cast<uint16_t>(static_cast<unsigned char>(s[p]) | (static_cast<unsigned char>(s[p + 1]) << 8));
}
uint32_t rd32(const std::string& s, size_t p) {
    if (p + 4 > s.size()) throw std::runtime_error("zip: truncated");
    uint32_t v = 0;
    for (int i = 3; i >= 0; --i) v = (v << 8) | static_cast<unsigned char>(s[p + static_cast<size_t>(i)]);
    return v;
}

// Fixed DOS date/time (1 January 2026 00:00): reproducible archives; the real
// generation time is in the manifest.
constexpr uint16_t kDosTime = 0;
constexpr uint16_t kDosDate = static_cast<uint16_t>(((2026 - 1980) << 9) | (1 << 5) | 1);
constexpr uint16_t kFlagUtf8 = 1u << 11;

std::string deflateRaw(const std::string& in) {
    z_stream z{};
    if (deflateInit2(&z, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("zip: deflateInit2");
    std::string out;
    out.resize(deflateBound(&z, static_cast<uLong>(in.size())));
    z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    z.avail_in = static_cast<uInt>(in.size());
    z.next_out = reinterpret_cast<Bytef*>(out.data());
    z.avail_out = static_cast<uInt>(out.size());
    int rc = deflate(&z, Z_FINISH);
    deflateEnd(&z);
    if (rc != Z_STREAM_END) throw std::runtime_error("zip: deflate");
    out.resize(z.total_out);
    return out;
}

std::string inflateRaw(const std::string& in, size_t expected) {
    z_stream z{};
    if (inflateInit2(&z, -15) != Z_OK) throw std::runtime_error("zip: inflateInit2");
    std::string out(expected, '\0');
    z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    z.avail_in = static_cast<uInt>(in.size());
    z.next_out = reinterpret_cast<Bytef*>(out.data());
    z.avail_out = static_cast<uInt>(out.size());
    int rc = inflate(&z, Z_FINISH);
    inflateEnd(&z);
    if (rc != Z_STREAM_END || z.total_out != expected) throw std::runtime_error("zip: inflate");
    return out;
}

void checkName(const std::string& n) {
    if (n.empty() || n.size() > 400 || n[0] == '/' || n.find("..") != std::string::npos || n.find('\\') != std::string::npos)
        throw std::invalid_argument("zip: invalid entry name " + n);
}

}  // namespace

void Writer::add(const std::string& name, const std::string& data, bool compress) {
    if (finished_) throw std::logic_error("zip: archive already finished");
    checkName(name);
    constexpr uint64_t kMax = std::numeric_limits<uint32_t>::max();
    if (data.size() >= kMax) throw std::length_error("zip: entry too large");
    Entry e;
    e.name = name;
    e.size = static_cast<uint32_t>(data.size());
    e.crc = static_cast<uint32_t>(crc32(0L, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size())));
    std::string body = compress ? deflateRaw(data) : std::string();
    if (!compress || body.size() >= data.size()) {
        body = data;
        e.method = 0;
    } else {
        e.method = 8;
    }
    e.compressedSize = static_cast<uint32_t>(body.size());
    if (out_.size() + body.size() + name.size() + 30 >= kMax) throw std::length_error("zip: archive too large");
    e.offset = static_cast<uint32_t>(out_.size());
    le32(out_, 0x04034b50);
    le16(out_, 20);
    le16(out_, kFlagUtf8);
    le16(out_, e.method);
    le16(out_, kDosTime);
    le16(out_, kDosDate);
    le32(out_, e.crc);
    le32(out_, e.compressedSize);
    le32(out_, e.size);
    le16(out_, static_cast<uint16_t>(name.size()));
    le16(out_, 0);
    out_ += name;
    out_ += body;
    entries_.push_back(std::move(e));
}

std::string Writer::finish() {
    if (finished_) throw std::logic_error("zip: archive already finished");
    if (entries_.size() >= 0xffff) throw std::length_error("zip: too many entries");
    finished_ = true;
    const uint32_t cdStart = static_cast<uint32_t>(out_.size());
    for (const auto& e : entries_) {
        le32(out_, 0x02014b50);
        le16(out_, (3 << 8) | 20);  // made by: UNIX, spec 2.0
        le16(out_, 20);
        le16(out_, kFlagUtf8);
        le16(out_, e.method);
        le16(out_, kDosTime);
        le16(out_, kDosDate);
        le32(out_, e.crc);
        le32(out_, e.compressedSize);
        le32(out_, e.size);
        le16(out_, static_cast<uint16_t>(e.name.size()));
        le16(out_, 0);
        le16(out_, 0);
        le16(out_, 0);
        le16(out_, 0);
        le32(out_, 0100644u << 16);  // regular file, rw-r--r--
        le32(out_, e.offset);
        out_ += e.name;
    }
    const uint32_t cdSize = static_cast<uint32_t>(out_.size()) - cdStart;
    le32(out_, 0x06054b50);
    le16(out_, 0);
    le16(out_, 0);
    le16(out_, static_cast<uint16_t>(entries_.size()));
    le16(out_, static_cast<uint16_t>(entries_.size()));
    le32(out_, cdSize);
    le32(out_, cdStart);
    le16(out_, 0);
    return std::move(out_);
}

std::vector<std::pair<std::string, std::string>> read(const std::string& a) {
    if (a.size() < 22) throw std::runtime_error("zip: too short");
    size_t eocd = a.size() - 22;
    if (rd32(a, eocd) != 0x06054b50) throw std::runtime_error("zip: no end record");
    uint16_t count = rd16(a, eocd + 10);
    size_t p = rd32(a, eocd + 16);
    std::vector<std::pair<std::string, std::string>> out;
    for (uint16_t i = 0; i < count; ++i) {
        if (rd32(a, p) != 0x02014b50) throw std::runtime_error("zip: bad central header");
        uint16_t method = rd16(a, p + 10);
        uint32_t crc = rd32(a, p + 16), csize = rd32(a, p + 20), size = rd32(a, p + 24);
        uint16_t nlen = rd16(a, p + 28), xlen = rd16(a, p + 30), clen = rd16(a, p + 32);
        uint32_t off = rd32(a, p + 42);
        if (p + 46 + nlen > a.size()) throw std::runtime_error("zip: truncated");
        std::string name = a.substr(p + 46, nlen);
        p += 46u + nlen + xlen + clen;
        if (rd32(a, off) != 0x04034b50) throw std::runtime_error("zip: bad local header");
        size_t data = off + 30u + rd16(a, off + 26) + rd16(a, off + 28);
        if (data + csize > a.size()) throw std::runtime_error("zip: truncated data");
        std::string body = a.substr(data, csize);
        std::string content = method == 8 ? inflateRaw(body, size) : method == 0 ? body : throw std::runtime_error("zip: method");
        if (static_cast<uint32_t>(crc32(0L, reinterpret_cast<const Bytef*>(content.data()),
                                        static_cast<uInt>(content.size()))) != crc)
            throw std::runtime_error("zip: CRC mismatch for " + name);
        out.emplace_back(std::move(name), std::move(content));
    }
    return out;
}

}  // namespace nh::zip
