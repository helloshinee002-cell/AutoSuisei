cmake_minimum_required(VERSION 3.25)

project(AutoPilot
    VERSION 1.0.1
    DESCRIPTION "Windows Automation Suite — Macro Recorder + Web Recorder + OCR"
    LANGUAGES CXX
)
# ^ PROJECT_VERSION = single source of truth ของ app version (inject เข้า C++ ผ่าน APP_VERSION
#   ใน src/gui/CMakeLists.txt). bump ที่นี่ แล้ว sync ให้ตรงที่ installer/AutoSuisei.iss + scripts/make_user_guide_pdf.py

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc /wd4324)
    add_compile_definitions(
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING
    )
endif()

# Dependencies (resolved by vcpkg manifest mode)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Gui Network)
find_package(Tesseract CONFIG REQUIRED)
find_package(Leptonica CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(Lua REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/recorder)
add_subdirectory(src/player)
add_subdirectory(src/ocr)
add_subdirectory(src/web)
add_subdirectory(src/scripting)
add_subdirectory(src/storage)
add_subdirectory(src/vision)
add_subdirectory(src/gui)
add_subdirectory(src/cli)
add_subdirectory(tests)
