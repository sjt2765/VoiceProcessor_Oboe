cmake_minimum_required(VERSION 3.22.1)

project("voiceprocessor_oboe")

add_subdirectory(oboe)


add_library(voiceprocessor_oboe SHARED
        AudioEngine.h
        AudioEngine.cpp
        AudioConfig.h
        native-lib.cpp)

target_include_directories(voiceprocessor_oboe PRIVATE
        $CMAKE_SOURCE_DIR/oboe/include
)

target_link_libraries(${CMAKE_PROJECT_NAME}

        oboe
        android
        log)