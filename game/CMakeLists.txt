# Pure game logic: no hal, no hardware. Builds for firmware and host.
# Header-only until M1/M2 add sources (bot.cpp is added in M2).
add_library(game INTERFACE)
target_include_directories(game INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
