qt_add_executable(AutoPilot
    main.cpp
    MainWindow.h
    MainWindow.cpp
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
)

set_target_properties(AutoPilot PROPERTIES
    WIN32_EXECUTABLE TRUE
)
