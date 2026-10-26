cmake_minimum_required(VERSION 3.21)
project(nhill VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The server only targets Linux hosts; the client targets Windows, Linux, Android.
option(NHILL_BUILD_SERVER "Build the backend (Linux)" ${UNIX})
option(NHILL_BUILD_CLIENT "Build the Qt client and admin console" ON)
option(NHILL_BUILD_TESTS  "Build tests" ON)
option(NHILL_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(ANDROID)
    set(NHILL_BUILD_SERVER OFF)
    set(NHILL_BUILD_TESTS OFF)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    # Partial aggregate initialisation of option structs is intentional.
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-pedantic -Wno-missing-field-initializers)
    if(NHILL_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

if(NHILL_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(common)

if(NHILL_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(NHILL_BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(NHILL_BUILD_TESTS)
    add_subdirectory(tests)
endif()

include(cmake/NhillPackaging.cmake)
