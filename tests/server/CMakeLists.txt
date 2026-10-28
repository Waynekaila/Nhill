add_executable(test_server
    harness.cpp
    test_platform.cpp
    test_billing.cpp
    test_commerce.cpp
    test_samples.cpp
    test_activity.cpp
    test_console.cpp
    test_pos.cpp
    test_password.cpp
    test_entitlements.cpp
    test_documents.cpp
)
target_compile_definitions(test_server PRIVATE NHILL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(test_server PRIVATE nhill_server_core GTest::gtest_main)
# Needs PostgreSQL 16 server binaries (initdb, pg_ctl) and psql.
add_test(NAME server COMMAND test_server)
set_tests_properties(server PROPERTIES TIMEOUT 900)

add_executable(test_setup test_setup.cpp)
target_compile_definitions(test_setup PRIVATE NHILL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(test_setup PRIVATE nhill_server_core GTest::gtest_main)
add_test(NAME setup COMMAND test_setup)
set_tests_properties(setup PROPERTIES TIMEOUT 600)
