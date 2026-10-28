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

// Migrations recorded in the database but unknown to this server: the database was
// migrated by a newer version. This server must not run on it (after a downgrade,
// restore the copy of the database taken before the update instead).
std::vector<std::string> unknownMigrations(const std::string& url, const std::string& dir);

}  // namespace nh
