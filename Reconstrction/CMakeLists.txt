cmake_minimum_required(VERSION 3.14)
project(speech_txt_demo CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to your whisper.cpp checkout (default: sibling folder named whisper.cpp)
set(WHISPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp" CACHE PATH "whisper.cpp source dir")
add_subdirectory(${WHISPER_DIR} whisper_build)

add_executable(speech_txt main.cpp)
target_include_directories(speech_txt PRIVATE ${WHISPER_DIR}/include)
target_link_libraries(speech_txt PRIVATE whisper)
