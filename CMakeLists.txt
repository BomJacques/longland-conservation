cmake_minimum_required(VERSION 3.22)

project(LonglandConservation VERSION 0.1.0 LANGUAGES C CXX)

option(LONGLAND_BUILD_PLUGIN "Build the VST3 target" ON)
set(LONGLAND_JUCE_PATH "" CACHE PATH "Path to a JUCE source checkout")

enable_testing()

add_library(LonglandPhysics STATIC
    Source/Physics/EnergyState.cpp
    Source/Physics/EnergyTransferGraph.cpp
    Source/Physics/ConservationSolver.cpp
    Source/Physics/ConservationEngine.cpp)
target_include_directories(LonglandPhysics PUBLIC Source)
target_compile_features(LonglandPhysics PUBLIC cxx_std_20)

add_library(LonglandDSPModel STATIC Source/DSP/DSPMapper.cpp)
target_link_libraries(LonglandDSPModel PUBLIC LonglandPhysics)
target_include_directories(LonglandDSPModel PUBLIC Source)
target_compile_features(LonglandDSPModel PUBLIC cxx_std_20)

add_executable(LonglandPhysicsTests Tests/PhysicsTests.cpp)
target_link_libraries(LonglandPhysicsTests PRIVATE LonglandPhysics LonglandDSPModel)
target_compile_features(LonglandPhysicsTests PRIVATE cxx_std_20)
add_test(NAME conservation-physics COMMAND LonglandPhysicsTests)

add_executable(LonglandDSPMapperTests Tests/DSPMapperTests.cpp)
target_link_libraries(LonglandDSPMapperTests PRIVATE LonglandDSPModel)
target_compile_features(LonglandDSPMapperTests PRIVATE cxx_std_20)
add_test(NAME dsp-mapper COMMAND LonglandDSPMapperTests)

if (MSVC)
    foreach(target LonglandPhysics LonglandDSPModel LonglandPhysicsTests LonglandDSPMapperTests)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    endforeach()
else()
    foreach(target LonglandPhysics LonglandDSPModel LonglandPhysicsTests LonglandDSPMapperTests)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endif()

if (LONGLAND_BUILD_PLUGIN)
    if (LONGLAND_JUCE_PATH)
        add_subdirectory("${LONGLAND_JUCE_PATH}" JUCE)
    else()
        find_package(JUCE CONFIG REQUIRED)
    endif()

    juce_add_plugin(LonglandConservation
        COMPANY_NAME "Longland Audio"
        BUNDLE_ID "com.longland.conservation"
        PLUGIN_MANUFACTURER_CODE Lngd
        PLUGIN_CODE Cnsv
        FORMATS VST3
        PRODUCT_NAME "Longland Conservation"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(LonglandConservation PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/ConsequenceProcessor.cpp
        Source/DSP/ReflectionProcessor.cpp
        Source/UI/EnergyFieldComponent.cpp
        Source/UI/PhysicsDebugPanel.cpp)
    target_link_libraries(LonglandConservation PRIVATE
        LonglandPhysics
        LonglandDSPModel
        juce::juce_audio_utils
        juce::juce_dsp
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
    target_compile_features(LonglandConservation PRIVATE cxx_std_20)
    target_compile_definitions(LonglandConservation PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    add_executable(LonglandDSPTests
        Tests/DSPTests.cpp
        Source/DSP/ConsequenceProcessor.cpp
        Source/DSP/ReflectionProcessor.cpp)
    target_link_libraries(LonglandDSPTests PRIVATE
        LonglandDSPModel
        juce::juce_audio_basics
        juce::juce_recommended_config_flags)
    target_compile_features(LonglandDSPTests PRIVATE cxx_std_20)
    add_test(NAME adversarial-dsp COMMAND LonglandDSPTests)

    add_executable(LonglandPluginSmokeTests Tests/PluginSmokeTests.cpp)
    target_link_libraries(LonglandPluginSmokeTests PRIVATE
        LonglandConservation
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
    target_compile_features(LonglandPluginSmokeTests PRIVATE cxx_std_20)
    target_compile_definitions(LonglandPluginSmokeTests PRIVATE
        LONGLAND_UI_SNAPSHOT="${CMAKE_BINARY_DIR}/Longland-Conservation-UI.png")
    add_test(NAME plugin-smoke COMMAND LonglandPluginSmokeTests)

    add_executable(LonglandVST3LoadTests Tests/VST3LoadTests.cpp)
    target_link_libraries(LonglandVST3LoadTests PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
    target_compile_features(LonglandVST3LoadTests PRIVATE cxx_std_20)
    target_compile_definitions(LonglandVST3LoadTests PRIVATE
        JUCE_PLUGINHOST_VST3=1
        LONGLAND_VST3_BUNDLE="${CMAKE_BINARY_DIR}/LonglandConservation_artefacts/$<CONFIG>/VST3/Longland Conservation.vst3")
    add_dependencies(LonglandVST3LoadTests LonglandConservation_VST3)
    add_test(NAME vst3-load COMMAND LonglandVST3LoadTests)
endif()
