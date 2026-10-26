#pragma once

#include <string>
#include <vector>

namespace nh::csv {

// RFC 4180 with ';' as default separator (opens correctly in French-locale
// spreadsheets). A UTF-8 BOM is prepended so accents display correctly.
inline std::string escape(const std::string& s, char sep) {
    bool quote = s.find_first_of(std::string("\"\r\n") + sep) != std::string::npos;
    // Neutralise spreadsheet formula injection (=, +, -, @ at start).
    std::string v = s;
    if (!v.empty() && (v[0] == '=' || v[0] == '+' || v[0] == '@' || (v[0] == '-' && v.size() > 1 && !isdigit(static_cast<unsigned char>(v[1])))))
        v = "'" + v;
    if (!quote) return v;
    std::string out = "\"";
    for (char c : v) {
        if (c == '"') out += "\"\"";
        else out.push_back(c);
    }
    return out + "\"";
}

class Writer {
public:
    explicit Writer(char sep = ';') : sep_(sep) { out_ = "\xEF\xBB\xBF"; }
    void row(const std::vector<std::string>& cells) {
        for (size_t i = 0; i < cells.size(); ++i) {
            if (i) out_.push_back(sep_);
            out_ += escape(cells[i], sep_);
        }
        out_ += "\r\n";
    }
    const std::string& str() const { return out_; }

private:
    char sep_;
    std::string out_;
};

// Parses CSV text (auto-detects ';' or ',' from the header line).
inline std::vector<std::vector<std::string>> parse(std::string text) {
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);
    auto firstNl = text.find('\n');
    std::string header = text.substr(0, firstNl);
    char sep = header.find(';') != std::string::npos ? ';' : ',';
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string cell;
    bool inQuotes = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    cell.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cell.push_back(c);
            }
        } else if (c == '"') {
            inQuotes = true;
        } else if (c == sep) {
            row.push_back(cell);
            cell.clear();
        } else if (c == '\r') {
            continue;
        } else if (c == '\n') {
            row.push_back(cell);
            cell.clear();
            bool empty = row.size() == 1 && row[0].empty();
            if (!empty) rows.push_back(row);
            row.clear();
        } else {
            cell.push_back(c);
        }
    }
    if (!cell.empty() || !row.empty()) {
        row.push_back(cell);
        rows.push_back(row);
    }
    return rows;
}

}  // namespace nh::csv
