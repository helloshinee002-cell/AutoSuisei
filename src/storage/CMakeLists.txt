add_library(autopilot_storage STATIC
    IMacroRepository.h
    IOcrResultRepository.h
    MacroSerializer.h
    MacroSerializer.cpp
    SqliteMacroRepository.h
    SqliteMacroRepository.cpp
    SqliteOcrResultRepository.h
    SqliteOcrResultRepository.cpp
)
target_include_directories(autopilot_storage PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(autopilot_storage
    PUBLIC
        autopilot_core
        nlohmann_json::nlohmann_json
    PRIVATE
        unofficial::sqlite3::sqlite3
)
