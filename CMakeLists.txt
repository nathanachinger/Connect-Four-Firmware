cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME ConnectFour)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C/C++ languages
enable_language(C CXX ASM)

# Warnings for our own code (not applied to CubeMX/HAL sources)
add_library(cf_warnings INTERFACE)
target_compile_options(cf_warnings INTERFACE -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion)

# Portable layers: build for both firmware and host
add_subdirectory(hal)
add_subdirectory(game)
add_subdirectory(devices)
add_subdirectory(app)

if(CMAKE_CROSSCOMPILING)
    # ---------------- Firmware (arm-none-eabi) ----------------
    add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
    )

    # Create an executable object type
    add_executable(${CMAKE_PROJECT_NAME})

    # Add STM32CubeMX generated sources
    add_subdirectory(cmake/stm32cubemx)

    # STM32-specific implementations + composition root
    add_subdirectory(board)

    # Remove wrong libob.a library dependency when using cpp files
    list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

    target_link_libraries(${CMAKE_PROJECT_NAME}
        stm32cubemx
        board
    )

    # Emit .hex/.bin and print size after linking
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    )
else()
    # ---------------- Host (unit tests, simulator) ----------------
    enable_testing()
    add_subdirectory(tests)
endif()
