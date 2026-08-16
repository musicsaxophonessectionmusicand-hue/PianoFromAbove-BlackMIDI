cmake_minimum_required(VERSION 3.22.1)
project(kivapfa_android LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CORE ${CMAKE_CURRENT_LIST_DIR}/../../../../native-core)
add_library(kivapfa SHARED native-lib.cpp
    ${CORE}/src/MidiParser.cpp
    ${CORE}/src/PagedNoteStore.cpp
    ${CORE}/src/PlaybackCursor.cpp)
target_include_directories(kivapfa PRIVATE ${CORE}/include)
target_compile_options(kivapfa PRIVATE -O3 -ffast-math)
