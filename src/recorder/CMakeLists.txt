add_library(autopilot_recorder STATIC
    IRecorder.h
    WindowsRecorder.h
    WindowsRecorder.cpp
)
target_include_directories(autopilot_recorder PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(autopilot_recorder
    PUBLIC
        autopilot_core
    PRIVATE
        spdlog::spdlog
        User32
)
