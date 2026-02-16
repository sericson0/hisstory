cmake_minimum_required(VERSION 3.22)
project(HisstoryVST VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch JUCE ───────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── Plugin Target ────────────────────────────────────────────────────────────
juce_add_plugin(HisstoryVST
    COMPANY_NAME            "Hisstory Audio"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Hist
    PLUGIN_CODE              Hsty
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "Hisstory"
    BUNDLE_ID                "com.HisstoryAudio.HisstoryVST"
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

# Generate the unified JuceHeader.h so source files can #include <JuceHeader.h>
juce_generate_juce_header(HisstoryVST)

# ── Source Files ─────────────────────────────────────────────────────────────
target_sources(HisstoryVST
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# ── Compile Definitions ─────────────────────────────────────────────────────
target_compile_definitions(HisstoryVST
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ── Link Libraries ──────────────────────────────────────────────────────────
target_link_libraries(HisstoryVST
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ── Test executable (offline de-hiss sanity check) ─────────────────────────
add_executable(TestDehiss
    Source/TestDehiss.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(TestDehiss PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JucePlugin_Name="Hisstory"
    JucePlugin_ManufacturerCode=0x48697374
    JucePlugin_PluginCode=0x48737479
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JucePlugin_EditorRequiresKeyboardFocus=0
)

target_include_directories(TestDehiss PRIVATE
    ${CMAKE_BINARY_DIR}/HisstoryVST_artefacts/JuceLibraryCode
)

target_link_libraries(TestDehiss PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)

# ── Benchmark executable (comparison test with RX 11) ──────────────────────
add_executable(Benchmark
    Source/Benchmark.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(Benchmark PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_PLUGINHOST_VST3=1
    JucePlugin_Name="Hisstory"
    JucePlugin_ManufacturerCode=0x48697374
    JucePlugin_PluginCode=0x48737479
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JucePlugin_EditorRequiresKeyboardFocus=0
)

target_include_directories(Benchmark PRIVATE
    ${CMAKE_BINARY_DIR}/HisstoryVST_artefacts/JuceLibraryCode
)

target_link_libraries(Benchmark PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
)
