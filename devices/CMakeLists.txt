# Chip drivers written against hal/ interfaces (IS31FL3731, ST7565, ...).
# Header-only until M4/M6 add sources.
add_library(devices INTERFACE)
target_include_directories(devices INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(devices INTERFACE hal)
