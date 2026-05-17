find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

set(AUTOPILOT_TEST_SOURCES
    test_main.cpp
    ocr/test_ocr_engine.cpp
    ocr/test_ocr_formatter.cpp
    ocr/test_asset_extractor.cpp
    ocr/test_review_model.cpp
    core/test_action.cpp
    storage/test_macro_serializer.cpp
    storage/test_macro_repository.cpp
    storage/test_ocr_result_repository.cpp
    web/test_cdp_message.cpp
    web/test_browser_event.cpp
    scripting/test_lua_sandbox.cpp
    vision/test_image_matcher.cpp
)

add_executable(autopilot_tests ${AUTOPILOT_TEST_SOURCES})
set_target_properties(autopilot_tests PROPERTIES
    AUTOMOC OFF
    AUTORCC OFF
    AUTOUIC OFF
)

target_link_libraries(autopilot_tests PRIVATE
    autopilot_core
    autopilot_ocr
    autopilot_storage
    autopilot_web
    autopilot_scripting
    autopilot_vision
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gmock
)

target_compile_definitions(autopilot_tests PRIVATE
    AUTOPILOT_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

# MSVC's /OPT:REF can DCE GoogleTest's static TEST() registrars when
# nothing references them at link time. Force keep all object-file
# data/sections for the test exe so registrations survive into the binary.
if(MSVC)
    target_link_options(autopilot_tests PRIVATE /OPT:NOREF /OPT:NOICF)
    # EXPECT_THROW discards [[nodiscard]] return values by design
    target_compile_options(autopilot_tests PRIVATE /wd4834)
endif()

# Set runtime env so test exe can find vcpkg DLLs + Tesseract traineddata
set(_dll_dir_dbg "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin")
set(_dll_dir_rel "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin")
gtest_discover_tests(autopilot_tests
    PROPERTIES
        ENVIRONMENT "PATH=${_dll_dir_dbg};${_dll_dir_rel};$ENV{PATH};TESSDATA_PREFIX=$ENV{TESSDATA_PREFIX}"
)
