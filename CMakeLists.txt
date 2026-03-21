cmake_minimum_required(VERSION 3.20)
project(WakoSound VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
pkg_check_modules(SNDFILE   REQUIRED sndfile)
pkg_check_modules(RUBBERBAND REQUIRED rubberband)

set(SOURCES
    src/main.cpp
    resources.qrc
    src/audio/AudioCache.cpp
    src/audio/PitchCache.cpp
    src/audio/VoicePool.cpp
    src/audio/Player.cpp
    src/model/KitManager.cpp
    src/sequencer/Pattern.cpp
    src/sequencer/Engine.cpp
    src/ui/MainWindow.cpp
    src/ui/PadGrid.cpp
    src/ui/StepGrid.cpp
    src/ui/TransportBar.cpp
    src/ui/SampleBrowser.cpp
    src/ui/MixerPanel.cpp
)

add_executable(WakoSound ${SOURCES})

target_include_directories(WakoSound PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${PORTAUDIO_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
    ${RUBBERBAND_INCLUDE_DIRS}
)

target_link_libraries(WakoSound PRIVATE
    Qt6::Widgets
    Qt6::Svg
    ${PORTAUDIO_LIBRARIES}
    ${SNDFILE_LIBRARIES}
    ${RUBBERBAND_LIBRARIES}
)

target_compile_options(WakoSound PRIVATE
    -Wall -Wextra -O2
    ${PORTAUDIO_CFLAGS_OTHER}
    ${SNDFILE_CFLAGS_OTHER}
    ${RUBBERBAND_CFLAGS_OTHER}
)

configure_file(
    ${CMAKE_SOURCE_DIR}/resources/kits/kits.json
    ${CMAKE_BINARY_DIR}/kits.json
    COPYONLY
)

install(TARGETS WakoSound RUNTIME DESTINATION bin)

add_custom_command(TARGET WakoSound POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/sounds
        ${CMAKE_BINARY_DIR}/sounds
)