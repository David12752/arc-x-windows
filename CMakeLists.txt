# ============================================================================
# ARC System Audio Host - CMake Configuration
# A professional VST3 plugin host for system-wide audio processing
# ============================================================================

cmake_minimum_required(VERSION 3.22)
project(ARCSystemAudioHost VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Windows specific
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
endif()

# ============================================================================
# JUCE Configuration
# ============================================================================

# Option to specify JUCE path
set(JUCE_PATH "" CACHE PATH "Path to JUCE framework")

if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} juce_build EXCLUDE_FROM_ALL)
else()
    # Try to fetch JUCE if not provided
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.9
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ============================================================================
# VST3 SDK Configuration
# ============================================================================

set(VST3_SDK_PATH "" CACHE PATH "Path to VST3 SDK")

if(VST3_SDK_PATH)
    set(SMTG_ADD_VST3_HOSTING_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ADD_VSTGUI OFF CACHE BOOL "" FORCE)
    set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${VST3_SDK_PATH} vst3sdk_build EXCLUDE_FROM_ALL)
endif()

# ============================================================================
# Source Files
# ============================================================================

set(AUDIO_ENGINE_SOURCES
    src/AudioEngine/AudioDeviceManager.cpp
    src/AudioEngine/AudioProcessor.cpp
    src/AudioEngine/PluginHost.cpp
    src/AudioEngine/WASAPILoopback.cpp
    src/AudioEngine/VirtualAudioDevice.cpp
    src/AudioEngine/PDCEngine.cpp
    src/AudioEngine/SRCEngine.cpp
    src/AudioEngine/VBCableWrapper.cpp
)

set(AUDIO_ENGINE_HEADERS
    src/AudioEngine/AudioDeviceManager.h
    src/AudioEngine/AudioProcessor.h
    src/AudioEngine/PluginHost.h
    src/AudioEngine/WASAPILoopback.h
    src/AudioEngine/VirtualAudioDevice.h
    src/AudioEngine/PDCEngine.h
    src/AudioEngine/SRCEngine.h
    src/AudioEngine/VBCableWrapper.h
)

set(VST_SOURCES
    src/VST/VSTScanner.cpp
    src/VST/VSTPlugin.cpp
    src/VST/VSTChain.cpp
    src/VST/PluginWindow.cpp
)

set(VST_HEADERS
    src/VST/VSTScanner.h
    src/VST/VSTPlugin.h
    src/VST/VSTChain.h
    src/VST/PluginWindow.h
)

set(UI_SOURCES
    src/UI/MainWindow.cpp
    src/UI/PluginBrowser.cpp
    src/UI/PluginChainView.cpp
    src/UI/SettingsDialog.cpp
    src/UI/VUMeter.cpp
    src/UI/SpectrumAnalyzer.cpp
    src/UI/LookAndFeel.cpp
    src/UI/PluginSlot.cpp
)

set(UI_HEADERS
    src/UI/MainWindow.h
    src/UI/PluginBrowser.h
    src/UI/PluginChainView.h
    src/UI/SettingsDialog.h
    src/UI/VUMeter.h
    src/UI/SpectrumAnalyzer.h
    src/UI/LookAndFeel.h
    src/UI/PluginSlot.h
)

set(UTILS_SOURCES
    src/Utils/PresetManager.cpp
    src/Utils/ConfigManager.cpp
    src/Utils/Logger.cpp
    src/Utils/CPUMonitor.cpp
    src/Utils/RTGuard.cpp
)

set(UTILS_HEADERS
    src/Utils/PresetManager.h
    src/Utils/ConfigManager.h
    src/Utils/Logger.h
    src/Utils/CPUMonitor.h
    src/Utils/RTGuard.h
)

set(MAIN_SOURCES
    src/main.cpp
    src/Application.cpp
)

set(MAIN_HEADERS
    src/Application.h
)

# ============================================================================
# Headless Engine — source groups (shared with arc-engine target)
# ============================================================================

set(CONCURRENCY_HEADERS
    src/Concurrency/SPSCQueue.h
    src/Concurrency/MetricsFrame.h
    src/Concurrency/AudioBlock.h
)

set(IPC_SOURCES
    src/IPC/CommandServer.cpp
    src/IPC/MetricsPublisher.cpp
)

set(IPC_HEADERS
    src/IPC/EngineCommand.h
    src/IPC/CommandServer.h
    src/IPC/MetricsPublisher.h
)

set(ENGINE_SOURCES
    src/Engine/AudioPipeline.cpp
    src/Engine/EngineCore.cpp
)

set(ENGINE_HEADERS
    src/Engine/AudioPipeline.h
    src/Engine/EngineCore.h
)

# ============================================================================
# ASIO SDK (Steinberg ASIO SDK 2.3) - optional, user must provide path.
#
# Download from: https://www.steinberg.net/asiosdk
# Then set at configure time:
#   cmake -DASIO_SDK_PATH=C:/ASIO_SDK ..
#
# When provided, both targets build with JUCE_ASIO=1.
# Without it, WASAPI-only build (JUCE_ASIO=0).
# ============================================================================
set(ASIO_SDK_PATH "" CACHE PATH "Path to Steinberg ASIO SDK 2.3 root folder")

if(ASIO_SDK_PATH)
    message(STATUS "ARC: ASIO SDK at ${ASIO_SDK_PATH} -- JUCE_ASIO=1")
    set(ARC_JUCE_ASIO_VALUE 1)
else()
    message(STATUS "ARC: No ASIO_SDK_PATH -- WASAPI only (JUCE_ASIO=0)")
    set(ARC_JUCE_ASIO_VALUE 0)
endif()

# ============================================================================
# JUCE Application Target
# ============================================================================

juce_add_gui_app(ARCSystemAudioHost
    PRODUCT_NAME "ARC System Audio Host"
    COMPANY_NAME "ARC Audio"
    COMPANY_COPYRIGHT "Copyright 2024 ARC Audio"
    VERSION "1.0.0"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE
)

target_sources(ARCSystemAudioHost
    PRIVATE
        ${MAIN_SOURCES}
        ${MAIN_HEADERS}
        ${AUDIO_ENGINE_SOURCES}
        ${AUDIO_ENGINE_HEADERS}
        ${VST_SOURCES}
        ${VST_HEADERS}
        ${UI_SOURCES}
        ${UI_HEADERS}
        ${UTILS_SOURCES}
        ${UTILS_HEADERS}
)

target_include_directories(ARCSystemAudioHost
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioEngine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/VST
        ${CMAKE_CURRENT_SOURCE_DIR}/src/UI
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Utils
)

# ============================================================================
# JUCE Module Dependencies
# ============================================================================

target_compile_definitions(ARCSystemAudioHost
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_PLUGINHOST_VST3=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=0
        JUCE_ASIO=${ARC_JUCE_ASIO_VALUE}
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_LV2=0
)

target_link_libraries(ARCSystemAudioHost
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Windows specific libraries
if(WIN32)
    target_link_libraries(ARCSystemAudioHost
        PRIVATE
            ole32
            oleaut32
            uuid
            winmm
            ksuser
            mfplat
            mfuuid
            wmcodecdspuuid
            avrt
    )
endif()

# ============================================================================
# VST3 SDK Linking (if available)
# ============================================================================

if(VST3_SDK_PATH)
    target_link_libraries(ARCSystemAudioHost
        PRIVATE
            sdk
            sdk_hosting
    )
    target_include_directories(ARCSystemAudioHost
        PRIVATE
            ${VST3_SDK_PATH}
    )
endif()

if(ASIO_SDK_PATH)
    target_include_directories(ARCSystemAudioHost
        PRIVATE
            ${ASIO_SDK_PATH}/common
    )
endif()

# ============================================================================
# Compiler Flags
# ============================================================================

if(MSVC)
    target_compile_options(ARCSystemAudioHost PRIVATE
        /W4
        /WX-
        /MP
        /Zc:__cplusplus
        /permissive-
        /EHsc
        $<$<CONFIG:Release>:/O2 /Ob2 /DNDEBUG>
        $<$<CONFIG:Debug>:/Od /Zi /DEBUG>
    )
    
    target_link_options(ARCSystemAudioHost PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
    )
else()
    target_compile_options(ARCSystemAudioHost PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Debug>:-O0 -g>
    )
endif()

# ============================================================================
# Resources
# ============================================================================

# Copy resources to build directory
file(GLOB_RECURSE RESOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")
foreach(RESOURCE ${RESOURCE_FILES})
    get_filename_component(RESOURCE_NAME ${RESOURCE} NAME)
    configure_file(${RESOURCE} ${CMAKE_BINARY_DIR}/resources/${RESOURCE_NAME} COPYONLY)
endforeach()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS ARCSystemAudioHost
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/
    DESTINATION share/ARCSystemAudioHost/resources
)

# ============================================================================
# CPack Configuration
# ============================================================================

set(CPACK_PACKAGE_NAME "ARCSystemAudioHost")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "ARC Audio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "System-wide VST3 audio processing host")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "ARC System Audio Host")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
endif()

include(CPack)

# ============================================================================
# arc-engine — Headless Console Application
# ============================================================================
# Separate binary that can be spawned by the Electron UI process.
# Links only non-GUI JUCE modules — no juce_gui_basics, no juce_graphics.
# Communicates with the UI via:
#   \\.\pipe\arc-command   (Named Pipe, inbound commands)
#   arc_metrics_shm        (Named Shared Memory, outbound metrics @ 60Hz)
#   \\.\pipe\arc-events    (Named Pipe, outbound event push)
# ============================================================================

juce_add_console_app(arc-engine
    PRODUCT_NAME    "ARC Engine"
    COMPANY_NAME    "ARC Audio"
    VERSION         "2.0.0"
)

target_sources(arc-engine
    PRIVATE
        src/engine_main.cpp
        ${AUDIO_ENGINE_SOURCES}
        ${AUDIO_ENGINE_HEADERS}
        ${VST_SOURCES}
        ${VST_HEADERS}
        ${UTILS_SOURCES}
        ${UTILS_HEADERS}
        ${CONCURRENCY_HEADERS}
        ${IPC_SOURCES}
        ${IPC_HEADERS}
        ${ENGINE_SOURCES}
        ${ENGINE_HEADERS}
)

target_include_directories(arc-engine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioEngine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/VST
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Utils
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Concurrency
        ${CMAKE_CURRENT_SOURCE_DIR}/src/IPC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine
)

target_compile_definitions(arc-engine
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_CAMERA=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=0
        JUCE_ASIO=${ARC_JUCE_ASIO_VALUE}
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_LV2=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_HEADLESS_PLUGIN_CLIENT=1
)

target_link_libraries(arc-engine
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(WIN32)
    target_link_libraries(arc-engine
        PRIVATE
            ole32
            oleaut32
            uuid
            winmm
            ksuser
            mfplat
            mfuuid
            wmcodecdspuuid
            avrt
    )
endif()

if(MSVC)
    target_compile_options(arc-engine PRIVATE
        /W4
        /WX-
        /MP
        /Zc:__cplusplus
        /permissive-
        /EHsc
        $<$<CONFIG:Release>:/O2 /Ob2 /DNDEBUG>
        $<$<CONFIG:Debug>:/Od /Zi /DEBUG>
    )
    target_link_options(arc-engine PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
    )
endif()

if(VST3_SDK_PATH)
    target_link_libraries(arc-engine PRIVATE sdk sdk_hosting)
    target_include_directories(arc-engine PRIVATE ${VST3_SDK_PATH})
endif()

if(ASIO_SDK_PATH)
    target_include_directories(arc-engine
        PRIVATE
            ${ASIO_SDK_PATH}/common
    )
endif()

install(TARGETS arc-engine RUNTIME DESTINATION bin)

# ============================================================================
# Unit Tests (Optional)
# ============================================================================

option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Print Configuration Summary
# ============================================================================

message(STATUS "")
message(STATUS "ARC System Audio Host Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  JUCE Path: ${JUCE_PATH}")
message(STATUS "  VST3 SDK Path: ${VST3_SDK_PATH}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "")

