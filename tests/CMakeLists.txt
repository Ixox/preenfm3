cmake_minimum_required(VERSION 3.20)

project(preenfm3_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_SANITIZERS "Build host tests with AddressSanitizer/UBSan" OFF)

set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src/app)
set(STORAGE_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/pfm3_preset_storage)

add_executable(preenfm3_tests
    test_main.cpp
    pfm3_preset_storage/format_contract_test.cpp
    pfm3_preset_storage/roundtrip_test.cpp
    pfm3_preset_storage/version_migration_test.cpp
    pfm3_preset_storage/arp_persistence_test.cpp
    pfm3_preset_storage/legacy_compatibility_test.cpp
    ${FW}/engine/pfm3/Pfm3FlashFormat.cpp
    ${FW}/engine/pfm3/Pfm3PresetIO.cpp
    ${FW}/engine/pfm3/Pfm3StorageBlob.cpp
)

target_include_directories(preenfm3_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/doctest
    ${STORAGE_TESTS}
    ${FW}
    ${FW}/synth
    ${FW}/engine/pfm3
)

target_compile_definitions(preenfm3_tests PRIVATE
    TEST_FIXTURE_DIR="${STORAGE_TESTS}/fixtures"
)

target_compile_options(preenfm3_tests PRIVATE
    -Wall
    -Wextra
    -fno-strict-aliasing
)

if(ENABLE_SANITIZERS)
    target_compile_options(preenfm3_tests PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(preenfm3_tests PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME preenfm3_tests COMMAND preenfm3_tests)
