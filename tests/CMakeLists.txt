# ============================================================================
# ARC System Audio Host - Unit Tests Configuration
# ============================================================================

# Option to enable/disable tests
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    # Fetch Catch2 for testing
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    # Test executable
    add_executable(ARCSystemAudioHostTests
        test_main.cpp
        test_CPUMonitor.cpp
        test_Constants.cpp
        test_ConfigManager.cpp
    )

    target_link_libraries(ARCSystemAudioHostTests
        PRIVATE
            Catch2::Catch2
            juce::juce_core
            juce::juce_data_structures
    )

    target_include_directories(ARCSystemAudioHostTests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/Utils
            ${CMAKE_SOURCE_DIR}/src/AudioEngine
    )

    # Add test sources from main project
    target_sources(ARCSystemAudioHostTests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src/Utils/CPUMonitor.cpp
            ${CMAKE_SOURCE_DIR}/src/Utils/ConfigManager.cpp
            ${CMAKE_SOURCE_DIR}/src/Utils/Logger.cpp
    )

    # Enable CTest
    include(CTest)
    catch_discover_tests(ARCSystemAudioHostTests)
endif()

