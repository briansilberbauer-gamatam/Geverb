cmake_minimum_required(VERSION 3.22)

project(Geverb VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SKIP_RPATH TRUE)

option(GEVERB_BUILD_TESTS "Build the offline DSP test executable" ON)
option(GEVERB_BUILD_SCREENSHOT "Build the headless editor renderer" ON)
option(GEVERB_BUILD_HOST_TEST "Build the VST3 host smoke test" ON)

# Use the offline JUCE checkout during local dsh builds, and FetchContent on CI.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(GeverbAssets
    SOURCES
        Resources/LatoRegular.ttf
        Resources/LatoSemibold.ttf
        Resources/LatoBlack.ttf
        Resources/Lato-OFL.txt)

set(GEVERB_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/Presets.h
    Source/Version.h
    Source/dsp/DelayLine.h
    Source/dsp/ReverbEngine.cpp
    Source/dsp/ReverbEngine.h
    Source/ui/Theme.cpp
    Source/ui/Theme.h
    Source/ui/StudioLookAndFeel.cpp
    Source/ui/StudioLookAndFeel.h
    Source/ui/Controls.cpp
    Source/ui/Controls.h
    Source/ui/WaveformDisplay.cpp
    Source/ui/WaveformDisplay.h)

set(GEVERB_DEFINITIONS
    GEVERB_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

juce_add_plugin(Geverb
    COMPANY_NAME "BLS Audio"
    BUNDLE_ID "com.blsaudio.geverb"
    PLUGIN_MANUFACTURER_CODE Blsa
    PLUGIN_CODE Gvrb
    FORMATS VST3 Standalone
    PRODUCT_NAME "Geverb"
    DESCRIPTION "Lush ambient plate, hall and shimmer reverb for lead guitar"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx Reverb")

target_sources(Geverb PRIVATE ${GEVERB_SOURCES})
target_include_directories(Geverb PRIVATE Source)
target_compile_definitions(Geverb PUBLIC ${GEVERB_DEFINITIONS})

target_link_libraries(Geverb
    PRIVATE
        GeverbAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(GEVERB_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(GeverbTests PRODUCT_NAME "GeverbTests")

    target_sources(GeverbTests PRIVATE ${GEVERB_SOURCES} Tests/DspTests.cpp)
    target_include_directories(GeverbTests PRIVATE Source)
    target_compile_definitions(GeverbTests PRIVATE ${GEVERB_DEFINITIONS})
    target_link_libraries(GeverbTests
        PRIVATE
            GeverbAssets
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME geverb_dsp COMMAND $<TARGET_FILE:GeverbTests>)
endif()

if(GEVERB_BUILD_HOST_TEST)
    juce_add_console_app(GeverbHostSmokeTest PRODUCT_NAME "GeverbHostSmokeTest")

    target_sources(GeverbHostSmokeTest PRIVATE Tools/HostSmokeTest.cpp)
    target_compile_definitions(GeverbHostSmokeTest PRIVATE
        ${GEVERB_DEFINITIONS}
        JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(GeverbHostSmokeTest
        PRIVATE
            juce::juce_audio_utils
            juce::juce_gui_extra
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_dependencies(GeverbHostSmokeTest Geverb_VST3)
endif()

if(GEVERB_BUILD_SCREENSHOT)
    juce_add_gui_app(GeverbScreenshot
        PRODUCT_NAME "Geverb Screenshot"
        COMPANY_NAME "BLS Audio")

    target_sources(GeverbScreenshot PRIVATE ${GEVERB_SOURCES} Tools/RenderEditor.cpp)
    target_include_directories(GeverbScreenshot PRIVATE Source)
    target_compile_definitions(GeverbScreenshot PRIVATE ${GEVERB_DEFINITIONS})

    target_link_libraries(GeverbScreenshot
        PRIVATE
            GeverbAssets
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
