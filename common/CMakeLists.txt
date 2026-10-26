add_library(nhill_common STATIC
    src/money.cpp
    src/establishment_code.cpp
    src/validation.cpp
)
target_include_directories(nhill_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(nhill_common PUBLIC cxx_std_17)
set_target_properties(nhill_common PROPERTIES POSITION_INDEPENDENT_CODE ON)
