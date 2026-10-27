# Real OS credential store test of "remember me" (Windows Credential Manager, Android
# Keystore, Secret Service). Built with the client tests, or alone with
# -DNHILL_BUILD_KEYCHAIN_TEST=ON (Windows and Android CI).
# Same packages as client/: imported targets (OpenGL, DBus, Qt6Keychain) are directory-scoped.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Svg Test)
find_package(Qt6Keychain CONFIG QUIET)
qt_add_executable(test_keychain test_keychain.cpp)
# qt_add_executable makes a Windows GUI-subsystem program by default: no console, so the
# test results would be lost. The test is a console program everywhere.
set_target_properties(test_keychain PROPERTIES AUTOMOC ON WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
target_link_libraries(test_keychain PRIVATE nhill_client_core Qt6::Test)
if(NOT ANDROID)
    add_test(NAME keychain COMMAND test_keychain)
    set_tests_properties(keychain PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
endif()
