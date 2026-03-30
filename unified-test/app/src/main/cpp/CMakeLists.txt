cmake_minimum_required(VERSION 3.22.1)
project("vmic_unified_test")

find_library(log-lib log)
find_library(android-lib android)

add_library(${CMAKE_PROJECT_NAME} SHARED
    audio_generator.cpp
    virtual_mic_writer.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    ${log-lib}
    ${android-lib}
)
