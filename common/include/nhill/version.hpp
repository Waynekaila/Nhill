// Single place where the product identity lives. Renaming the product
// (see docs/00-etude-conception.md §9, trademark risk) starts here.
#pragma once

namespace nhill {

inline constexpr const char* kProductName = "Nhill";
inline constexpr const char* kProductVersion = "2.0.0";
inline constexpr const char* kApiVersion = "v1";
// Oldest client version the server accepts (semantic version). 2.0.0 keeps API v1
// unchanged, so 0.1.x clients stay accepted (see docs/fr/changelog.md).
inline constexpr const char* kMinClientVersion = "0.1.0";

}  // namespace nhill
