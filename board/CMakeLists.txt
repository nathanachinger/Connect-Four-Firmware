# STM32 implementations of hal/ interfaces + composition root (app_main).
# The only library allowed to include STM32 HAL / CubeMX headers.
add_library(board STATIC
    board_main.cpp
    stm32_gpio.cpp
    stm32_clock.cpp
)
target_include_directories(board PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(board PUBLIC hal app stm32cubemx PRIVATE cf_warnings)
