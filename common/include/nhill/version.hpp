// Single place where the product identity lives. Renaming the product
// (see docs/00-etude-conception.md §9, trademark risk) starts here.
#pragma once

namespace nhill {

inline constexpr const char* kProductName = "Nhill";
inline constexpr const char* kProductVersion = "3.0.0";
inline constexpr const char* kApiVersion = "v1";
// Oldest client version the server accepts (semantic version), announced by GET /meta.
// 3.0.0: the applications must hide every feature outside the plan of the establishment
// (rights from /me); 2.x applications do not, so they are asked to update (the server
// refuses those features anyway). See docs/fr/changelog.md.
inline constexpr const char* kMinClientVersion = "3.0.0";

// Publisher contacts shown by the applications (help screens, installers). The public web
// address is not written here: the applications never link to a site whose HTTPS has not
// been checked, and the API address is always the one entered by the user.
inline constexpr const char* kPublisherName = "Nhill Solution";
inline constexpr const char* kContactEmail = "info@nhill.online";
inline constexpr const char* kSupportEmail = "support@nhill.online";

}  // namespace nhill
