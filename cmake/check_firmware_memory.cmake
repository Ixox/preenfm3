# Post-link firmware memory budget check.
#
# Invoked from firmware/CMakeLists.txt with:
#   -DSIZE_TOOL=<arm-none-eabi-size> -DELF=<firmware elf>
#
# Fails the build when a RAM region is above the threshold, so a new large
# buffer cannot silently take the last few kilobytes. See doc/memory_map.md.

if(NOT DEFINED SIZE_TOOL OR NOT DEFINED ELF)
    message(FATAL_ERROR "check_firmware_memory.cmake: SIZE_TOOL and ELF are required")
endif()

execute_process(
    COMMAND ${SIZE_TOOL} -A ${ELF}
    OUTPUT_VARIABLE size_output
    RESULT_VARIABLE size_result)
if(NOT size_result EQUAL 0)
    message(FATAL_ERROR "check_firmware_memory.cmake: size tool failed on ${ELF}")
endif()

string(REPLACE "\n" ";" size_lines "${size_output}")

set(dtcm 0)
set(ram_d1 0)
set(ram_d2 0)
set(ram_d2b 0)
set(ram_d2c 0)
set(ram_d3 0)
set(itcm 0)
set(flash 0)

foreach(line IN LISTS size_lines)
    # Section lines are "<name> <size> <address>". The header and the
    # "Total" line have only two fields, so they are skipped.
    if(line MATCHES "^ *([A-Za-z0-9_.]+) +([0-9]+) +[0-9]+$")
        set(section "${CMAKE_MATCH_1}")
        set(bytes "${CMAKE_MATCH_2}")
        if(section MATCHES "^\\.(text|rodata|isr_vector|ARM|init_array|fini_array|preinit_array)")
            math(EXPR flash "${flash} + ${bytes}")
        elseif(section MATCHES "^\\.(data|bss)$" OR section STREQUAL "._user_heap_stack")
            math(EXPR dtcm "${dtcm} + ${bytes}")
            if(section STREQUAL ".data")
                # .data is copied from a flash load image.
                math(EXPR flash "${flash} + ${bytes}")
            endif()
        elseif(section STREQUAL ".ram_d1")
            math(EXPR ram_d1 "${ram_d1} + ${bytes}")
        elseif(section STREQUAL ".ram_d2")
            math(EXPR ram_d2 "${ram_d2} + ${bytes}")
        elseif(section STREQUAL ".ram_d2b")
            math(EXPR ram_d2b "${ram_d2b} + ${bytes}")
        elseif(section STREQUAL ".ram_d2c")
            math(EXPR ram_d2c "${ram_d2c} + ${bytes}")
        elseif(section STREQUAL ".ram_d3")
            math(EXPR ram_d3 "${ram_d3} + ${bytes}")
        elseif(section STREQUAL ".instruction_ram")
            math(EXPR itcm "${itcm} + ${bytes}")
        endif()
    endif()
endforeach()

if(NOT DEFINED PRE_ENFM3_MEMORY_THRESHOLD)
    set(PRE_ENFM3_MEMORY_THRESHOLD 95)
endif()
set(threshold ${PRE_ENFM3_MEMORY_THRESHOLD})

macro(check_region name used limit region_threshold)
    math(EXPR percent "(${used} * 100) / ${limit}")
    message(STATUS "${name}: ${used} / ${limit} B (${percent}%)")
    if(percent GREATER region_threshold)
        message(SEND_ERROR "${name} is at ${percent}% (limit ${region_threshold}%); see doc/memory_map.md")
    endif()
endmacro()

check_region(DTCMRAM ${dtcm} 131072 ${threshold})
check_region(RAM_D1 ${ram_d1} 524288 ${threshold})
check_region(RAM_D2 ${ram_d2} 131072 ${threshold})
check_region(RAM_D2B ${ram_d2b} 131072 ${threshold})
check_region(RAM_D2C ${ram_d2c} 32768 ${threshold})
check_region(RAM_D3 ${ram_d3} 65536 ${threshold})
check_region(ITCMRAM ${itcm} 64512 ${threshold})
check_region(FLASH ${flash} 1966080 ${threshold})