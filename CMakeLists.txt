cmake_minimum_required(VERSION 3.20)

project(preenfm3 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party)
add_subdirectory(preen_lib)
add_subdirectory(bootloader)
add_subdirectory(firmware)

# ---------------------------------------------------------------------------
# Host tests. The firmware build is cross-compiled, so the tests configure a
# separate native build tree with the default (host) compiler.
# From the build directory: make test / make test-asan
# ---------------------------------------------------------------------------

find_program(CTEST_EXECUTABLE ctest REQUIRED)
set(HOST_TESTS_DIR "${CMAKE_BINARY_DIR}/host-tests")
set(HOST_TESTS_ASAN_DIR "${CMAKE_BINARY_DIR}/host-tests-asan")

add_custom_target(test
    COMMAND ${CMAKE_COMMAND}
        -S "${CMAKE_SOURCE_DIR}/tests"
        -B "${HOST_TESTS_DIR}"
        -DCMAKE_BUILD_TYPE=Debug
    COMMAND ${CMAKE_COMMAND} --build "${HOST_TESTS_DIR}" --parallel
    COMMAND "${CTEST_EXECUTABLE}" --test-dir "${HOST_TESTS_DIR}" --output-on-failure
    COMMENT "Building and running host tests"
    USES_TERMINAL
)

add_custom_target(test-asan
    COMMAND ${CMAKE_COMMAND}
        -S "${CMAKE_SOURCE_DIR}/tests"
        -B "${HOST_TESTS_ASAN_DIR}"
        -DCMAKE_BUILD_TYPE=Debug
        -DENABLE_SANITIZERS=ON
    COMMAND ${CMAKE_COMMAND} --build "${HOST_TESTS_ASAN_DIR}" --parallel
    COMMAND "${CTEST_EXECUTABLE}" --test-dir "${HOST_TESTS_ASAN_DIR}" --output-on-failure
    COMMENT "Building and running host tests with ASan/UBSan"
    USES_TERMINAL
)

# ---------------------------------------------------------------------------
# Versioned release folder: build/pfm3_firmware_<version>/ + .zip
# ---------------------------------------------------------------------------

find_program(ZIP_EXECUTABLE zip)

set(RELEASE_DIR "${CMAKE_BINARY_DIR}/pfm3_firmware_${FIRMWARE_VERSION_FILE}")
set(RELEASE_ZIP "${CMAKE_BINARY_DIR}/pfm3_firmware_${FIRMWARE_VERSION_FILE}.zip")
set(FW_BIN "p3_${FIRMWARE_VERSION_FILE}.bin")
set(BL_BIN "p3_boot_${BOOTLOADER_VERSION_FILE}.bin")
set(RELEASE_SCRIPT_DIR "${CMAKE_BINARY_DIR}/pfm3_release_scripts")
file(MAKE_DIRECTORY "${RELEASE_SCRIPT_DIR}")

configure_file("${CMAKE_SOURCE_DIR}/scripts/flash_firmware_dfu.cmd.in"
    "${RELEASE_SCRIPT_DIR}/flash_firmware_dfu.cmd" @ONLY NEWLINE_STYLE LF)
configure_file("${CMAKE_SOURCE_DIR}/scripts/flash_bootloader_dfu.cmd.in"
    "${RELEASE_SCRIPT_DIR}/flash_bootloader_dfu.cmd" @ONLY NEWLINE_STYLE LF)
configure_file("${CMAKE_SOURCE_DIR}/scripts/flash_firmware_stlink.cmd.in"
    "${RELEASE_SCRIPT_DIR}/flash_firmware_stlink.cmd" @ONLY NEWLINE_STYLE LF)
configure_file("${CMAKE_SOURCE_DIR}/scripts/flash_bootloader_stlink.cmd.in"
    "${RELEASE_SCRIPT_DIR}/flash_bootloader_stlink.cmd" @ONLY NEWLINE_STYLE LF)

set(RELEASE_ZIP_COMMANDS)
if(ZIP_EXECUTABLE)
    set(RELEASE_ZIP_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E rm -f "${RELEASE_ZIP}"
        COMMAND ${CMAKE_COMMAND} -E chdir "${RELEASE_DIR}"
            "${ZIP_EXECUTABLE}" -r "${RELEASE_ZIP}"
            "${FW_BIN}" "${BL_BIN}"
            flash_firmware_dfu.cmd flash_bootloader_dfu.cmd
            flash_firmware_stlink.cmd flash_bootloader_stlink.cmd)
else()
    message(WARNING "zip not found: ${RELEASE_ZIP} will not be created")
endif()

add_custom_target(pfm3_release ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${RELEASE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:preenfm3_firmware>/${FW_BIN}"
        "${RELEASE_DIR}/${FW_BIN}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:preenfm3_bootloader>/${BL_BIN}"
        "${RELEASE_DIR}/${BL_BIN}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RELEASE_SCRIPT_DIR}/flash_firmware_dfu.cmd"
        "${RELEASE_DIR}/flash_firmware_dfu.cmd"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RELEASE_SCRIPT_DIR}/flash_bootloader_dfu.cmd"
        "${RELEASE_DIR}/flash_bootloader_dfu.cmd"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RELEASE_SCRIPT_DIR}/flash_firmware_stlink.cmd"
        "${RELEASE_DIR}/flash_firmware_stlink.cmd"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RELEASE_SCRIPT_DIR}/flash_bootloader_stlink.cmd"
        "${RELEASE_DIR}/flash_bootloader_stlink.cmd"
    ${RELEASE_ZIP_COMMANDS}
    DEPENDS preenfm3_firmware preenfm3_bootloader
    COMMENT "Creating ${RELEASE_DIR}"
)
