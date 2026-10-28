#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nh::pdf {

struct Fonts {
    std::string regular;   // TTF paths (UTF-8 capable)
    std::string semibold;
    std::string mono;
    static Fonts fromDir(const std::string& dir);
};

// Returns true if libharu can decode the image (PNG or JPEG).
bool probeImage(const std::string& bytes, const std::string& mime);

using KV = std::pair<std::string, std::string>;

struct Line {
    std::string ref, description, qty, unit, unitPrice, discount, tax, total;
};

struct SalesDocView {
    std::string title;           // "FACTURE"
    std::string number;          // empty for drafts
    std::string watermark;       // e.g. "BROUILLON — SANS VALEUR", empty if none
    std::string sellerName;
    std::vector<std::string> sellerLines;
    std::string buyerLabel;      // "Facturé à"
    std::string buyerName;
    std::vector<std::string> buyerLines;
    std::optional<std::string> logoBytes;
    std::string logoMime;
    std::vector<KV> meta;        // label/value pairs under the title
    std::vector<std::string> columns;   // 7 headers: ref, description, qty, unit price, discount, tax, total
    std::vector<Line> lines;
    std::vector<KV> totals;      // before grand total
    KV grandTotal;
    std::vector<KV> afterTotals; // paid / balance
    std::string paymentsTitle;   // "Paiements reçus"
    std::vector<KV> payments;    // method · date -> amount (left of the totals)
    std::vector<std::string> notices;
    std::vector<KV> footnotes;   // payment terms, payment info, notes
    std::string footerLeft;      // "Généré par Nhill · Empreinte …"
    std::string pageLabel;       // "Page"
    std::string creationDateIso; // fixed PDF creation date -> deterministic output
};

std::string renderSalesDocument(const SalesDocView& v, const Fonts& fonts);

struct TableReport {
    std::string title;
    std::string subtitle;        // period
    std::string establishmentName;
    std::optional<std::string> logoBytes;
    std::string logoMime;
    std::vector<std::string> columns;
    std::vector<bool> rightAlign;
    std::vector<double> widths;  // relative widths
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> totalRow;   // optional
    std::string footerLeft;
    std::string pageLabel;
    bool landscape = false;
};

std::string renderTable(const TableReport& r, const Fonts& fonts);

// Narrow roll (thermal printer) document: receipts and cash register reports.
struct TicketBlock {
    enum Kind { Title, Center, Text, Muted, KV, KVStrong, Item, Rule, Space };
    Kind kind = Text;
    std::string a, b, c;   // Item: a = description, b = "qty × unit price", c = line total
};
struct TicketView {
    float widthMm = 80;                  // 58 or 80 mm rolls
    std::optional<std::string> logoBytes;
    std::string logoMime;
    std::string title;                   // PDF metadata title
    std::vector<TicketBlock> blocks;
    std::string creationDateIso;
};
std::string renderTicket(const TicketView& v, const Fonts& fonts);

}  // namespace nh::pdf
