# Post-build check: fails the build if the flash image reaches the settings page
# (app/flash_store.hpp: last 2 KB of flash, 0x0800F800).
#
# Done after linking instead of in the linker script so STM32G071xx_FLASH.ld stays exactly as
# CubeMX generates it. The image ends after the startup copy of .data/.tdata, which follows the
# code: _sidata + (_edata - _sdata).
#
# Usage: cmake -DNM=<arm-none-eabi-nm> -DELF=<file.elf> -P check_settings_page.cmake

set(SETTINGS_PAGE 0x0800F800)

execute_process(COMMAND ${NM} ${ELF} OUTPUT_VARIABLE symbols RESULT_VARIABLE nm_result)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "settings page check: could not read symbols from ${ELF}")
endif()

foreach(name _sidata _sdata _edata)
    string(REGEX MATCH "([0-9a-fA-F]+) [A-Za-z] ${name}\n" match "${symbols}")
    if(NOT match)
        message(FATAL_ERROR "settings page check: symbol ${name} not found in ${ELF}")
    endif()
    set(${name} "0x${CMAKE_MATCH_1}")
endforeach()

math(EXPR image_end "${_sidata} + ${_edata} - ${_sdata}" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR room "${SETTINGS_PAGE} - (${image_end})")
if(room LESS 0)
    message(FATAL_ERROR "Flash image ends at ${image_end}, overlapping the settings page at "
                        "${SETTINGS_PAGE} (see app/flash_store.hpp). Free up flash or move the page.")
endif()
message(STATUS "Flash image ends at ${image_end}; ${room} bytes free below the settings page")
