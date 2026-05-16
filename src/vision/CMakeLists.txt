add_library(autopilot_vision STATIC
    ImageMatcher.h
    ImageMatcher.cpp
)
target_include_directories(autopilot_vision PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(autopilot_vision
    PUBLIC
        autopilot_core
    PRIVATE
        ${OpenCV_LIBS}
        spdlog::spdlog
)
