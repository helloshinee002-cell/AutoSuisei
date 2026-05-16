add_executable(autopilot_cli main.cpp)
target_link_libraries(autopilot_cli PRIVATE
    autopilot_core
    autopilot_ocr
    autopilot_storage
    spdlog::spdlog
)
