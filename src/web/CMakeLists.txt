find_package(libwebsockets CONFIG REQUIRED)

add_library(autopilot_web STATIC
    ICdpClient.h
    CdpMessage.h
    CdpMessage.cpp
    CdpClient.h
    CdpClient.cpp
    WebPlayer.h
    WebPlayer.cpp
    BrowserRecorderScript.h
    BrowserRecorderScript.cpp
    BrowserEvent.h
    BrowserEvent.cpp
    CdpRecorder.h
    CdpRecorder.cpp
)
target_include_directories(autopilot_web PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(autopilot_web
    PUBLIC
        autopilot_core
        nlohmann_json::nlohmann_json
    PRIVATE
        websockets_shared
        spdlog::spdlog
)
