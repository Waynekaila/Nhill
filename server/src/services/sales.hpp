#pragma once

#include "nhill/money.hpp"
#include "services/common.hpp"

namespace nh {

// Invoice status from its amounts (shared by sales and payments).
std::string invoiceStatusFor(int64_t total, int64_t paid, int64_t credited);
json loadSalesDocument(db::Tx& tx, const std::string& id);
std::string renderSalesPdf(Env& env, db::Tx& tx, const std::string& id);
// Document date in the reader's convention.
std::string formatDate(const std::string& iso, const std::string& lang);

// Building blocks shared by Business sales and Nhill POS.
namespace sales {
struct LineCalc {
    std::optional<std::string> productId;
    std::string description, unit, taxCode;
    int64_t qty = 0, unitPrice = 0;
    int discountBp = 0, taxBp = 0;
};
// Parses and prices b["lines"] (server-side prices, taxes and rounding).
std::pair<std::vector<LineCalc>, nhill::DocumentTotals> priceLines(db::Tx& tx, const json& b, const std::string& currency,
                                                                   const std::string& rounding);
void writeLines(db::Tx& tx, const std::string& est, const std::string& docId, const std::vector<LineCalc>& lines,
                const nhill::DocumentTotals& t);
json breakdownJson(const nhill::DocumentTotals& t);
void history(db::Tx& tx, const Actor& actor, const std::string& docId, const std::optional<std::string>& from,
             const std::string& to, const std::string& note = "");
// Issues a draft (numbering, snapshots, stock in `warehouseId` or the default warehouse,
// credit applied to the original document, PDF fingerprint). Returns the document.
json issue(Env& env, db::Tx& tx, const Actor& actor, const std::string& id, const json& b,
           const std::optional<std::string>& warehouseId = std::nullopt);
}  // namespace sales

}  // namespace nh
