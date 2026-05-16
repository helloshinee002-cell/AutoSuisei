find_package(sol2 CONFIG REQUIRED)
find_package(Lua REQUIRED)

add_library(autopilot_scripting STATIC
    LuaSandbox.h
    LuaSandbox.cpp
)
target_include_directories(autopilot_scripting PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(autopilot_scripting
    PUBLIC
        autopilot_core
    PRIVATE
        sol2::sol2
        ${LUA_LIBRARIES}
        spdlog::spdlog
)
