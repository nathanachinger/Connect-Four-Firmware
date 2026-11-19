# Hardware abstraction interfaces. Header-only, no platform dependencies.
add_library(hal INTERFACE)
target_include_directories(hal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
