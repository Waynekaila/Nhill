#pragma once

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace nh {

// An error that is safe to show to the client. `code` is a stable machine
// identifier that the client translates; `message` is an English hint for
// developers, never containing secrets or other tenants' data.
struct ApiError : std::runtime_error {
    int status;
    std::string code;
    nlohmann::json details;
    ApiError(int s, std::string c, const std::string& msg, nlohmann::json d = nullptr)
        : std::runtime_error(msg), status(s), code(std::move(c)), details(std::move(d)) {}
};

inline ApiError badRequest(const std::string& code, const std::string& msg = "invalid request",
                           nlohmann::json d = nullptr) {
    return ApiError(400, code, msg, std::move(d));
}
inline ApiError invalid(const std::string& field, const std::string& reason) {
    return ApiError(422, "validation_failed", "invalid field: " + field, {{"field", field}, {"reason", reason}});
}
inline ApiError notFound(const std::string& what = "resource") {
    return ApiError(404, "not_found", what + " not found");
}
inline ApiError forbidden(const std::string& code = "forbidden") {
    return ApiError(403, code, "operation not permitted");
}
inline ApiError conflict(const std::string& code, const std::string& msg = "conflict", nlohmann::json d = nullptr) {
    return ApiError(409, code, msg, std::move(d));
}
inline ApiError unauthorized(const std::string& code = "unauthorized") {
    return ApiError(401, code, "authentication required");
}
inline ApiError tooMany() { return ApiError(429, "too_many_attempts", "too many attempts, retry later"); }

}  // namespace nh
