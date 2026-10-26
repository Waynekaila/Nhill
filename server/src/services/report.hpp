// Shared tabular report engine: the same rows are returned as JSON, CSV or PDF
// (?format=json|csv|pdf). Money columns are never summed across currencies.
#pragma once

#include "services/services.hpp"

namespace nh {

enum class Col { Text, Money, Qty, Date, Int };

struct Report {
    std::string titleKey;
    std::vector<std::string> keys;     // i18n keys "col.*"
    std::vector<Col> types;
    std::vector<int> currencyCol;      // per column: index of the currency column for Money, or -1
    std::vector<std::vector<std::string>> rows;
    bool landscape = false;
    // Header of the PDF: when empty, the current establishment (name + logo).
    std::string orgName;
    std::string extraSubtitle;
};

struct Period {
    std::string from, to;
};

Period period(const Ctx& ctx);
std::string normalizeSpaces(std::string s);
void sendReport(Env& env, Ctx& ctx, db::Tx& tx, const Report& r, const std::string& name, const Period* per);

}  // namespace nh
