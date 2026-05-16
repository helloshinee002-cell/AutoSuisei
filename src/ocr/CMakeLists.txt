add_library(autopilot_ocr STATIC
    OcrEngine.h
    OcrEngine.cpp
    OcrFormatter.h
    OcrFormatter.cpp
)

target_include_directories(autopilot_ocr PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(autopilot_ocr
    PUBLIC
        autopilot_core
        nlohmann_json::nlohmann_json
    PRIVATE
        ${OpenCV_LIBS}
        Tesseract::libtesseract
        leptonica
        spdlog::spdlog
)
