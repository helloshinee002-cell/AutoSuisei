add_library(autopilot_player STATIC
    IPlayer.h
    WindowsPlayer.h
    WindowsPlayer.cpp
)
target_include_directories(autopilot_player PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(autopilot_player
    PUBLIC
        autopilot_core
    PRIVATE
        spdlog::spdlog
        User32
)
