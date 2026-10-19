# CMake build for SIDStep2 (Linux, and any other platform JUCE's CMake supports).
#
# Dependencies live in Requirements/ (gitignored), mirroring the original Projucer layout:
#   Requirements/JUCE        JUCE 8.x  (git clone --branch 8.0.15 https://github.com/juce-framework/JUCE.git)
#   Requirements/resid-0.16  reSID 0.16 (https://www.zimmers.net/anonftp/pub/cbm/crossplatform/emulators/resid/)
#
# Build:
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Formats: VST3, LV2 and Standalone by default. VST2 is built only when you
# point SIDSTEP2_VST2_SDK at a folder containing the Steinberg VST2 SDK
# (pluginterfaces/vst2.x/aeffect.h), which Steinberg no longer distributes.

cmake_minimum_required(VERSION 3.22)

project(SIDStep2 VERSION 1.0.0 LANGUAGES C CXX)

set(SIDSTEP2_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Requirements/JUCE" CACHE PATH "Path to a JUCE 8 checkout")
set(SIDSTEP2_RESID_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Requirements/resid-0.16" CACHE PATH "Path to the reSID 0.16 sources")
set(SIDSTEP2_VST2_SDK "" CACHE PATH "Optional: path to the VST2 SDK, enables the VST2 format")
option(SIDSTEP2_COPY_AFTER_BUILD "Install plugins into ~/.vst3, ~/.lv2 (and ~/.vst) after each build" OFF)

if(NOT EXISTS "${SIDSTEP2_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${SIDSTEP2_JUCE_DIR}. Clone it with:\n"
                        "  git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git Requirements/JUCE")
endif()
if(NOT EXISTS "${SIDSTEP2_RESID_DIR}/sid.h")
    message(FATAL_ERROR "reSID 0.16 not found at ${SIDSTEP2_RESID_DIR}. Download and extract resid-0.16.tar.gz into Requirements/")
endif()

add_subdirectory("${SIDSTEP2_JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)

set(SIDSTEP2_FORMATS VST3 LV2 Standalone)
if(SIDSTEP2_VST2_SDK)
    juce_set_vst2_sdk_path("${SIDSTEP2_VST2_SDK}")
    list(APPEND SIDSTEP2_FORMATS VST)
endif()

# ---------------------------------------------------------------------------
# reSID 0.16, built as a small static library.
file(GLOB RESID_SOURCES CONFIGURE_DEPENDS "${SIDSTEP2_RESID_DIR}/*.cc")
add_library(resid STATIC ${RESID_SOURCES})
target_include_directories(resid PUBLIC "${SIDSTEP2_RESID_DIR}")
target_compile_features(resid PRIVATE cxx_std_17)
# Linked into the plugin shared objects.
set_target_properties(resid PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(resid PRIVATE VERSION="0.16")
target_compile_options(resid PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-w>)

# ---------------------------------------------------------------------------
juce_add_plugin(SIDStep2
    PRODUCT_NAME "SIDStep2"
    COMPANY_NAME "BAS"
    BUNDLE_ID "com.BAS.SIDStep2"
    PLUGIN_MANUFACTURER_CODE BAS_
    PLUGIN_CODE SDST
    DESCRIPTION "SID chip synthesizer (reSID) with C64 song export"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    LV2URI "urn:bas:sidstep2"
    COPY_PLUGIN_AFTER_BUILD ${SIDSTEP2_COPY_AFTER_BUILD}
    FORMATS ${SIDSTEP2_FORMATS})

file(GLOB_RECURSE SIDSTEP2_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")
target_sources(SIDStep2 PRIVATE ${SIDSTEP2_SOURCES})
target_compile_features(SIDStep2 PRIVATE cxx_std_17)

# The sources include "<JuceHeader.h>"; generate one for this target.
juce_generate_juce_header(SIDStep2)

target_compile_definitions(SIDStep2
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # The exporter uses FileChooser::browseForFileToSave (a modal loop).
        JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(SIDStep2
    PRIVATE
        resid
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
