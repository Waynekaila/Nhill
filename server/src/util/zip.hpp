#pragma once
// Minimal ZIP writer (PKWARE APPNOTE 6.3.10, sections 4.3 and 4.4): DEFLATE entries
// (zlib, raw stream) or STORED ones, UTF-8 names (general purpose bit 11), no ZIP64,
// so the whole archive stays below 4 GiB (checked). Used for data exports that any
// spreadsheet or archive tool opens.
#include <cstdint>
#include <string>
#include <vector>

namespace nh::zip {

class Writer {
public:
    // Adds a file. Names use '/' as separator, are relative, without "..".
    void add(const std::string& name, const std::string& data, bool compress = true);
    // Archive bytes (local headers, data, central directory, end record).
    std::string finish();
    size_t size() const { return out_.size(); }

private:
    struct Entry {
        std::string name;
        uint32_t crc = 0, compressedSize = 0, size = 0, offset = 0;
        uint16_t method = 0;
    };
    std::vector<Entry> entries_;
    std::string out_;
    bool finished_ = false;
};

// Reads an archive written by Writer (tests and verification tools): name -> content.
// Throws std::runtime_error on a malformed archive or a CRC mismatch.
std::vector<std::pair<std::string, std::string>> read(const std::string& archive);

}  // namespace nh::zip
