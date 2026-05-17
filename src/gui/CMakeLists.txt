qt_add_executable(AutoPilot
    main.cpp
    MainWindow.h
    MainWindow.cpp
    MacrosTab.h
    MacrosTab.cpp
    OcrTab.h
    OcrTab.cpp
    WebTab.h
    WebTab.cpp
    ImageTab.h
    ImageTab.cpp
    ReviewTab.h
    ReviewTab.cpp
    WatchTab.h
    WatchTab.cpp
)

target_link_libraries(AutoPilot PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    autopilot_core
    autopilot_ocr
    autopilot_storage
    autopilot_recorder
    autopilot_player
    autopilot_web
    autopilot_vision
    nlohmann_json::nlohmann_json
)

# Bake the project's scripts/ path into the exe so it can find bulk_extract.py
# at runtime without manual env-var setup. AUTOPILOT_SCRIPTS_DIR env var still
# overrides this if set.
target_compile_definitions(AutoPilot PRIVATE
    AUTOPILOT_SCRIPTS_DIR="${CMAKE_SOURCE_DIR}/scripts"
)

set_target_properties(AutoPilot PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Deploy Qt platform plugins next to the exe so it can find qwindows(d).dll
# vcpkg's windeployqt has trouble locating debug DLLs, so we copy plugin
# folders directly. Configuration-aware: pick debug vs release plugin set.
if(WIN32)
    set(_qt_plugin_root_debug
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/Qt6/plugins")
    set(_qt_plugin_root_release
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/Qt6/plugins")
    set(_qt_plugin_root "$<IF:$<CONFIG:Debug>,${_qt_plugin_root_debug},${_qt_plugin_root_release}>")

    foreach(_subdir platforms styles imageformats)
        add_custom_command(TARGET AutoPilot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_qt_plugin_root}/${_subdir}"
                "$<TARGET_FILE_DIR:AutoPilot>/${_subdir}"
            COMMENT "Deploying Qt plugin: ${_subdir}"
            VERBATIM
        )
    endforeach()
endif()
