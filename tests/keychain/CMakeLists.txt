# Real OS credential store test of "remember me" (Windows Credential Manager, Android
# Keystore, Secret Service). Built with the client tests, or alone with
# -DNHILL_BUILD_KEYCHAIN_TEST=ON (Windows and Android CI).
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Network Qml Test)
qt_add_executable(test_keychain test_keychain.cpp)
set_target_properties(test_keychain PROPERTIES AUTOMOC ON)
target_link_libraries(test_keychain PRIVATE nhill_client_core Qt6::Test)
if(NOT ANDROID)
    add_test(NAME keychain COMMAND test_keychain)
    set_tests_properties(keychain PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
endif()
