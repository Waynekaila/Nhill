find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQXX REQUIRED IMPORTED_TARGET libpqxx)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json 3.10 REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(HTTPLIB REQUIRED IMPORTED_TARGET cpp-httplib)
# SMTP delivery of e-mails (password recovery codes, notices): libcurl with TLS.
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
find_path(HPDF_INCLUDE_DIR hpdf.h REQUIRED)
find_library(HPDF_LIBRARY NAMES hpdf REQUIRED)

# libpqxx and cpp-httplib from the distribution are compiled libraries built in
# C++17 mode: the whole project uses C++17 to keep their ABI (see docs/technique.md).

# Everything except main.cpp goes in a library so that tests link the real code.
add_library(nhill_server_core STATIC
    src/config.cpp
    src/db.cpp
    src/migrate.cpp
    src/setup.cpp
    src/routes.cpp
    src/http/app.cpp
    src/util/crypto.cpp
    src/util/mailer.cpp
    src/pdf/pdf.cpp
    src/jobs/jobs.cpp
    src/services/common.cpp
    src/services/report.cpp
    src/services/activity.cpp
    src/services/codes.cpp
    src/services/auth.cpp
    src/services/establishments.cpp
    src/services/members.cpp
    src/services/subscriptions.cpp
    src/services/providers.cpp
    src/services/console.cpp
    src/services/support.cpp
    src/services/pos.cpp
    src/services/catalog.cpp
    src/services/parties.cpp
    src/services/stock.cpp
    src/services/sales.cpp
    src/services/purchases.cpp
    src/services/payments.cpp
    src/services/expenses.cpp
    src/services/reports.cpp
    src/services/admin.cpp
)
target_include_directories(nhill_server_core PUBLIC src ${HPDF_INCLUDE_DIR})
target_link_libraries(nhill_server_core PUBLIC
    nhill_common
    PkgConfig::LIBPQXX
    PkgConfig::SODIUM
    PkgConfig::HTTPLIB
    PkgConfig::PNG
    PkgConfig::CURL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ${HPDF_LIBRARY}
    Threads::Threads
)

add_executable(nhill-server src/main.cpp)
target_link_libraries(nhill-server PRIVATE nhill_server_core)

include(GNUInstallDirs)
install(TARGETS nhill-server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)
install(DIRECTORY profiles/ DESTINATION ${CMAKE_INSTALL_DATADIR}/nhill/profiles COMPONENT server)
install(DIRECTORY i18n/ DESTINATION ${CMAKE_INSTALL_DATADIR}/nhill/i18n COMPONENT server)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/db/migrations/ DESTINATION ${CMAKE_INSTALL_DATADIR}/nhill/migrations COMPONENT server)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/db/bootstrap/ DESTINATION ${CMAKE_INSTALL_DATADIR}/nhill/bootstrap COMPONENT server)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/fonts/ DESTINATION ${CMAKE_INSTALL_DATADIR}/nhill/fonts COMPONENT server
        FILES_MATCHING PATTERN "*.ttf" PATTERN "*.txt")
if(UNIX AND NOT APPLE)
    # Service files, backup scripts and configuration examples (no secret).
    install(FILES ${PROJECT_SOURCE_DIR}/deploy/systemd/nhill-server.service ${PROJECT_SOURCE_DIR}/deploy/systemd/nhill-backup.service
                  ${PROJECT_SOURCE_DIR}/deploy/systemd/nhill-backup.timer
            DESTINATION lib/systemd/system COMPONENT server)
    install(PROGRAMS ${PROJECT_SOURCE_DIR}/deploy/backup/nhill-backup.sh ${PROJECT_SOURCE_DIR}/deploy/backup/nhill-restore.sh
            DESTINATION ${CMAKE_INSTALL_SBINDIR} COMPONENT server)
    install(FILES ${PROJECT_SOURCE_DIR}/config/server.env.example ${PROJECT_SOURCE_DIR}/deploy/backup/backup.env.example
            DESTINATION ${CMAKE_INSTALL_DATADIR}/nhill/examples COMPONENT server)
    install(FILES ${PROJECT_SOURCE_DIR}/deploy/caddy/Caddyfile DESTINATION ${CMAKE_INSTALL_DATADIR}/nhill/examples
            RENAME Caddyfile.example COMPONENT server)
endif()
