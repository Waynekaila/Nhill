#pragma once

#include <string>
#include <vector>

namespace nh {

struct MigrationResult {
    std::vector<std::string> applied;
    std::vector<std::string> alreadyApplied;
};

// Applies db/migrations/*.sql in lexical order, each in its own transaction,
// under an advisory lock. A migration whose content changed after being
// applied aborts the run (checksum mismatch).
MigrationResult runMigrations(const std::string& ownerUrl, const std::string& dir);

// Returns the list of pending migrations without applying them.
std::vector<std::string> pendingMigrations(const std::string& url, const std::string& dir);

}  // namespace nh
