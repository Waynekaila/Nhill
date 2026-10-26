find_package(GTest REQUIRED)

add_executable(test_common common/test_common.cpp)
target_link_libraries(test_common PRIVATE nhill_common GTest::gtest_main)
add_test(NAME common COMMAND test_common)

if(NHILL_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(NHILL_BUILD_CLIENT)
    add_subdirectory(client)
endif()
