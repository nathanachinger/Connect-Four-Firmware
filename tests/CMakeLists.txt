# Host-only unit tests. Never built for the firmware.
include(FetchContent)

find_package(GTest 1.14 CONFIG QUIET)
if(NOT GTest_FOUND)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
        GIT_SHALLOW    TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

add_library(test_fakes INTERFACE)
target_include_directories(test_fakes INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/fakes)
target_link_libraries(test_fakes INTERFACE hal)

add_executable(unit_tests
    test_app.cpp
    test_rules.cpp
)
target_link_libraries(unit_tests PRIVATE app game test_fakes GTest::gtest_main cf_warnings)
gtest_discover_tests(unit_tests)
