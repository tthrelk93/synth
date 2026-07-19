cmake_minimum_required(VERSION 3.24)

project(ModelD VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SYNTH_JUCE_SOURCE_DIR "" CACHE PATH
    "Optional path to a local JUCE checkout at the required revision")
option(SYNTH_BUILD_TESTS "Build developer and CI tests" ON)
option(SYNTH_BUILD_VALIDATORS "Build optional plug-in validators" OFF)
set(SYNTH_VALIDATION_REPEAT_COUNT "3" CACHE STRING
    "Number of fresh instances exercised by project-owned validators")
option(SYNTH_WARNINGS_AS_ERRORS "Treat project-source warnings as errors" OFF)
option(SYNTH_ENABLE_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
option(SYNTH_CI_INJECT_FAILING_SENTINEL
       "Register the deterministic CI CTest failure sentinel" OFF)
option(SYNTH_VALIDATE_DISTRIBUTION_IDENTITY
       "Require approved legal identity for distribution validation" OFF)
set(SYNTH_STAGE_ROOT "${CMAKE_BINARY_DIR}/development-stage" CACHE PATH
    "Root for non-distribution development artifact staging")
set(SYNTH_TEST_REPORTS "" CACHE STRING
    "Semicolon-separated test reports beneath this build tree to declare in the development manifest")

include(cmake/ProductIdentity.cmake)

set(_synth_identity_errors "")
if(SYNTH_MANUFACTURER_NAME STREQUAL ""
   OR SYNTH_MANUFACTURER_NAME STREQUAL "yourcompany")
    list(APPEND _synth_identity_errors "manufacturer name is empty or placeholder")
endif()

if(SYNTH_MANUFACTURER_DOMAIN STREQUAL ""
   OR SYNTH_MANUFACTURER_DOMAIN MATCHES "(^|\\.)yourcompany($|\\.)"
   OR SYNTH_MANUFACTURER_DOMAIN MATCHES "(^|\\.)example\\.(com|net|org)$")
    list(APPEND _synth_identity_errors "reverse-DNS domain is empty or placeholder")
endif()

if(NOT SYNTH_IDENTITY_APPROVED)
    list(APPEND _synth_identity_errors "identity is not approved")
endif()

if(SYNTH_VALIDATE_DISTRIBUTION_IDENTITY AND _synth_identity_errors)
    list(JOIN _synth_identity_errors "; " _synth_identity_error_text)
    message(FATAL_ERROR
        "Distribution identity validation failed (BLD-007): ${_synth_identity_error_text}.")
endif()

include(cmake/Dependencies.cmake)

add_library(SynthProjectOptions INTERFACE)
target_compile_features(SynthProjectOptions INTERFACE cxx_std_20)

function(synth_enable_project_warnings)
    if(MSVC)
        set(_synth_warning_options /W4)
        if(SYNTH_WARNINGS_AS_ERRORS)
            list(APPEND _synth_warning_options /WX)
        endif()
    else()
        set(_synth_warning_options -Wall -Wextra -Wpedantic)
        if(SYNTH_WARNINGS_AS_ERRORS)
            list(APPEND _synth_warning_options -Werror)
        endif()
    endif()

    foreach(_synth_source IN LISTS ARGN)
        set_property(SOURCE "${_synth_source}" APPEND PROPERTY
            COMPILE_OPTIONS ${_synth_warning_options})
    endforeach()
endfunction()

if(SYNTH_ENABLE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "SYNTH_ENABLE_SANITIZERS is not supported with MSVC")
    endif()
    target_compile_options(SynthProjectOptions INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(SynthProjectOptions INTERFACE -fsanitize=address,undefined)
endif()

set(MODEL_D_CORE_SOURCES
    Source/CircularBuffer.cpp
    Source/EnvelopeGenerator.cpp
    Source/LadderFilter.cpp
    Source/ModWheel.cpp
    Source/Oscillator.cpp
    Source/PresetLibrary.cpp
    Source/SynthPreset.cpp)

synth_enable_project_warnings(${MODEL_D_CORE_SOURCES})

add_library(ModelDCore STATIC ${MODEL_D_CORE_SOURCES})
add_library(ModelD::Core ALIAS ModelDCore)
target_include_directories(ModelDCore
    PUBLIC
        Source
        cmake/core_compat)
target_compile_definitions(ModelDCore
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
target_link_libraries(ModelDCore
    PUBLIC
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
    PRIVATE
        SynthProjectOptions
        juce::juce_recommended_config_flags)

juce_add_binary_data(ModelDAssets
    NAMESPACE BinaryData
    SOURCES
        Source/blackPlastic.jpg
        Source/brooding.png
        Source/dalle.png
        Source/dalleBG.png
        Source/synthBack.png
        Source/synthBack2.png
        Source/synthBrooding.png
        Source/woodgrain.jpeg)

set(_model_d_formats VST3 Standalone)
if(APPLE)
    list(APPEND _model_d_formats AU)
endif()

juce_add_plugin(ModelDPlugin
    COMPANY_NAME "${SYNTH_MANUFACTURER_NAME}"
    COMPANY_WEBSITE "${SYNTH_MANUFACTURER_WEBSITE}"
    COMPANY_EMAIL "${SYNTH_MANUFACTURER_EMAIL}"
    BUNDLE_ID "${SYNTH_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE "${SYNTH_MANUFACTURER_CODE}"
    PLUGIN_CODE "${SYNTH_PRODUCT_CODE}"
    PRODUCT_NAME "${SYNTH_PRODUCT_NAME}"
    VERSION "${PROJECT_VERSION}"
    FORMATS ${_model_d_formats}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

set(MODEL_D_PLUGIN_SOURCES
        Source/BlackBackgroundComponent.cpp
        Source/CustomPanelComponent.cpp
        Source/CustomSliderLookAndFeel.cpp
        Source/ModWheelSlider.cpp
        Source/PianoKey.cpp
        Source/PitchWheelLookAndFeel.cpp
        Source/PitchWheelSlider.cpp
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
        Source/PresetManager.cpp
        Source/SignalFlowOverlay.cpp
        Source/SmokeComponent.cpp
        Source/VintageLookAndFeel.cpp
        Source/WaveformDisplay.cpp
        Source/WaveformSlider.cpp)
target_sources(ModelDPlugin PRIVATE ${MODEL_D_PLUGIN_SOURCES})
synth_enable_project_warnings(${MODEL_D_PLUGIN_SOURCES})
target_include_directories(ModelDPlugin
    BEFORE PRIVATE
        Source
        cmake/plugin_compat)
target_compile_definitions(ModelDPlugin
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
target_link_libraries(ModelDPlugin
    PRIVATE
        ModelDCore
        ModelDAssets
        SynthProjectOptions
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags)

function(synth_get_plugin_artifact output_variable target_name)
    get_target_property(_synth_artifact_path "${target_name}" JUCE_PLUGIN_ARTEFACT_FILE)
    if(NOT _synth_artifact_path OR _synth_artifact_path MATCHES "-NOTFOUND$")
        message(FATAL_ERROR
            "Required JUCE_PLUGIN_ARTEFACT_FILE property is missing for ${target_name}")
    endif()
    set("${output_variable}" "${_synth_artifact_path}" PARENT_SCOPE)
endfunction()

synth_get_plugin_artifact(_model_d_vst3_artifact ModelDPlugin_VST3)
synth_get_plugin_artifact(_model_d_standalone_artifact ModelDPlugin_Standalone)
set(_model_d_stage_dependencies ModelDPlugin_VST3 ModelDPlugin_Standalone)
string(REPLACE ";" "\\;" _model_d_test_reports_command_argument "${SYNTH_TEST_REPORTS}")
string(REPLACE ";" "\\;" _model_d_expected_formats_command_argument "${_model_d_formats}")
if(APPLE)
    set(_model_d_sdk_identifier "macosx")
    execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-version
        RESULT_VARIABLE _model_d_sdk_version_status
        OUTPUT_VARIABLE _model_d_sdk_version
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_model_d_sdk_version_status OR _model_d_sdk_version STREQUAL "")
        set(_model_d_sdk_version "system-default")
    endif()
else()
    set(_model_d_sdk_identifier "${CMAKE_SYSTEM_NAME}-system-default")
    set(_model_d_sdk_version "${CMAKE_SYSTEM_VERSION}")
endif()
set(_model_d_stage_arguments
    "-DSYNTH_STAGE_ROOT=${SYNTH_STAGE_ROOT}"
    "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_ROOT}/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
    "-DSYNTH_STAGE_VST3_ARTIFACT=${_model_d_vst3_artifact}"
    "-DSYNTH_STAGE_STANDALONE_ARTIFACT=${_model_d_standalone_artifact}"
    "-DSYNTH_SOURCE_ROOT=${CMAKE_SOURCE_DIR}"
    "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
    "-DSYNTH_PROJECT_VERSION=${PROJECT_VERSION}"
    "-DSYNTH_REQUIRED_JUCE_COMMIT=${SYNTH_REQUIRED_JUCE_COMMIT}"
    "-DSYNTH_RESOLVED_JUCE_COMMIT=${SYNTH_RESOLVED_JUCE_COMMIT}"
    "-DSYNTH_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
    "-DSYNTH_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}"
    "-DSYNTH_SDK_IDENTIFIER=${_model_d_sdk_identifier}"
    "-DSYNTH_SDK_VERSION=${_model_d_sdk_version}"
    "-DSYNTH_CONFIGURATION=$<CONFIG>"
    "-DSYNTH_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
    "-DSYNTH_ARCHITECTURE=${CMAKE_SYSTEM_PROCESSOR}"
    "-DSYNTH_PRODUCT_NAME=${SYNTH_PRODUCT_NAME}"
    "-DSYNTH_MANUFACTURER_NAME=${SYNTH_MANUFACTURER_NAME}"
    "-DSYNTH_MANUFACTURER_DOMAIN=${SYNTH_MANUFACTURER_DOMAIN}"
    "-DSYNTH_BUNDLE_ID=${SYNTH_BUNDLE_ID}"
    "-DSYNTH_MANUFACTURER_CODE=${SYNTH_MANUFACTURER_CODE}"
    "-DSYNTH_PRODUCT_CODE=${SYNTH_PRODUCT_CODE}"
    "-DSYNTH_IDENTITY_APPROVED=${SYNTH_IDENTITY_APPROVED}"
    "-DSYNTH_TEST_REPORTS=${_model_d_test_reports_command_argument}")
if(APPLE)
    synth_get_plugin_artifact(_model_d_au_artifact ModelDPlugin_AU)
    list(APPEND _model_d_stage_dependencies ModelDPlugin_AU)
    list(APPEND _model_d_stage_arguments "-DSYNTH_STAGE_AU_ARTIFACT=${_model_d_au_artifact}")
endif()

add_custom_target(ModelDStageDevelopmentArtifacts
    DEPENDS ${_model_d_stage_dependencies}
    COMMAND "${CMAKE_COMMAND}" ${_model_d_stage_arguments}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/StageDevelopmentArtifacts.cmake"
    COMMENT "Staging Model D development artifacts and deterministic build manifest"
    VERBATIM)

add_custom_target(ModelDVerifyStagedArtifacts
    DEPENDS ModelDStageDevelopmentArtifacts
    COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_ROOT}/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DSYNTH_EXPECTED_FORMATS=${_model_d_expected_formats_command_argument}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyStagedArtifacts.cmake"
    COMMENT "Verifying Model D development artifact staging manifest"
    VERBATIM)

if(SYNTH_BUILD_VALIDATORS)
    if(NOT SYNTH_VALIDATION_REPEAT_COUNT MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR
            "SYNTH_VALIDATION_REPEAT_COUNT must be a positive integer")
    endif()

    add_executable(ModelDActualWrapperSmoke
        Tools/ModelDActualWrapperSmoke.cpp)
    synth_enable_project_warnings(Tools/ModelDActualWrapperSmoke.cpp)
    target_compile_definitions(ModelDActualWrapperSmoke
        PRIVATE
            JUCE_PLUGINHOST_AU=0
            JUCE_PLUGINHOST_VST3=1
            JUCE_MODAL_LOOPS_PERMITTED=1
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            SYNTH_EXPECTED_PLUGIN_NAME="${SYNTH_PRODUCT_NAME}"
            SYNTH_VALIDATOR_TOOL_VERSION="${PROJECT_VERSION}")
    target_link_libraries(ModelDActualWrapperSmoke
        PRIVATE
            SynthProjectOptions
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_cryptography
            juce::juce_recommended_config_flags)

    set(_model_d_wrapper_smoke_report
        "${CMAKE_BINARY_DIR}/validation/actual-wrapper-smoke-report.json")
    set(_model_d_wrapper_smoke_log
        "${CMAKE_BINARY_DIR}/validation/actual-wrapper-smoke.log")
    add_custom_target(ModelDRunActualWrapperSmoke
        DEPENDS
            ModelDActualWrapperSmoke
            ModelDStageDevelopmentArtifacts
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_SMOKE_EXECUTABLE=$<TARGET_FILE:ModelDActualWrapperSmoke>"
            "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_ROOT}/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_REPEAT_COUNT=${SYNTH_VALIDATION_REPEAT_COUNT}"
            "-DSYNTH_SEED=0x4d6f64656c44"
            "-DSYNTH_REPORT_PATH=${_model_d_wrapper_smoke_report}"
            "-DSYNTH_LOG_PATH=${_model_d_wrapper_smoke_log}"
            "-DSYNTH_CONFIGURATION=$<CONFIG>"
            "-DSYNTH_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
            "-DSYNTH_ARCHITECTURE=${CMAKE_SYSTEM_PROCESSOR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunActualWrapperSmoke.cmake"
        COMMENT "Scanning and exercising the staged Model D VST3 wrapper"
        VERBATIM)
endif()

add_executable(ModelDOfflineRenderer Tools/ModelDOfflineRenderer.cpp)
synth_enable_project_warnings(Tools/ModelDOfflineRenderer.cpp)
target_link_libraries(ModelDOfflineRenderer PRIVATE ModelDCore SynthProjectOptions)

if(SYNTH_BUILD_TESTS)
    enable_testing()
    add_executable(ModelDTests Tests/ModelDTests.cpp)
    synth_enable_project_warnings(Tests/ModelDTests.cpp)
    target_include_directories(ModelDTests
        BEFORE PRIVATE
            Source
            cmake/plugin_compat
            "${juce_SOURCE_DIR}/modules")
    target_link_libraries(ModelDTests
        PRIVATE
            ModelDPlugin
            SynthProjectOptions)

    function(synth_add_model_d_test test_name test_label test_mode)
        add_test(NAME "${test_name}" COMMAND ModelDTests "${test_mode}")
        set_tests_properties("${test_name}" PROPERTIES LABELS "${test_label}")
    endfunction()

    synth_add_model_d_test(ModelDUnitContract unit unit)
    synth_add_model_d_test(ModelDStateSmoke state state)
    synth_add_model_d_test(ModelDDspContract dsp dsp)
    synth_add_model_d_test(ModelDMidiSampleZero midi midi)
    synth_add_model_d_test(ModelDRealtimeSmoke realtime realtime)
    synth_add_model_d_test(ModelDHostContract host host)

    set(_model_d_artifact_test_arguments
        "-DVST3_ARTIFACT=${_model_d_vst3_artifact}"
        "-DSTANDALONE_ARTIFACT=${_model_d_standalone_artifact}")
    if(APPLE)
        synth_get_plugin_artifact(_model_d_au_artifact ModelDPlugin_AU)
        list(APPEND _model_d_artifact_test_arguments
             "-DAU_ARTIFACT=${_model_d_au_artifact}")
    endif()
    add_test(NAME ModelDFormatArtifacts
        COMMAND ${CMAKE_COMMAND} ${_model_d_artifact_test_arguments}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake")
    set_tests_properties(ModelDFormatArtifacts PROPERTIES LABELS artifact)

    if(SYNTH_CI_INJECT_FAILING_SENTINEL)
        add_test(NAME ModelDCiFailingSentinel
            COMMAND ${CMAKE_COMMAND}
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/FailingSentinel.cmake")
        set_tests_properties(ModelDCiFailingSentinel PROPERTIES LABELS sentinel)
    endif()
endif()
