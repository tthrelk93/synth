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
set(SYNTH_PLUGINVAL_EXECUTABLE "" CACHE FILEPATH
    "Optional pluginval executable override (must report exactly version 1.0.4)")
set(SYNTH_PLUGINVAL_ARCHIVE "" CACHE FILEPATH
    "Optional local official pluginval v1.0.4 archive for reproducible/offline provisioning")
set(SYNTH_VST3_VALIDATOR_EXECUTABLE "" CACHE FILEPATH
    "Optional Steinberg VST3 SDK validator executable passed to pluginval")
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
set(SYNTH_PRODUCT_IDENTITY_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProductIdentity.cmake" CACHE FILEPATH
    "CMake file defining the product identity validated by the BLD-007 guard")

if(NOT EXISTS "${SYNTH_PRODUCT_IDENTITY_FILE}")
    message(FATAL_ERROR
        "SYNTH_PRODUCT_IDENTITY_FILE does not exist: ${SYNTH_PRODUCT_IDENTITY_FILE}")
endif()
include("${SYNTH_PRODUCT_IDENTITY_FILE}")
include(cmake/ValidateProductIdentity.cmake)

synth_collect_product_identity_errors(_synth_identity_errors)

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
# JUCE module sources propagate to ModelDCore consumers, so module-wide
# configuration switches must propagate with them.
target_compile_definitions(ModelDCore
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
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
        Source/ParameterRegistry.cpp
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

target_sources(ModelDPlugin_Standalone PRIVATE Source/StandaloneApp.cpp)
synth_enable_project_warnings(Source/StandaloneApp.cpp)
target_include_directories(ModelDPlugin_Standalone
    BEFORE PRIVATE
        Source
        cmake/plugin_compat)
target_compile_definitions(ModelDPlugin_Standalone
    PRIVATE
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
        SYNTH_ENABLE_STANDALONE_LIFECYCLE_TESTS=$<BOOL:${SYNTH_BUILD_TESTS}>)

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
set(_model_d_build_inventory_directory
    "${CMAKE_BINARY_DIR}/model-d-build-inventory")
file(MAKE_DIRECTORY "${_model_d_build_inventory_directory}")
set(_model_d_test_reports_file
    "${_model_d_build_inventory_directory}/declared-reports.txt")
set(_model_d_expected_formats_file
    "${_model_d_build_inventory_directory}/expected-formats.txt")
file(WRITE "${_model_d_test_reports_file}" "")
foreach(_report IN LISTS SYNTH_TEST_REPORTS)
    if(_report MATCHES "[\r\n]")
        message(FATAL_ERROR
            "Test report paths must not contain line breaks: ${_report}")
    endif()
    file(APPEND "${_model_d_test_reports_file}" "${_report}\n")
endforeach()
file(WRITE "${_model_d_expected_formats_file}" "")
foreach(_format IN LISTS _model_d_formats)
    if(_format MATCHES "[\r\n]")
        message(FATAL_ERROR
            "Expected artifact formats must not contain line breaks: ${_format}")
    endif()
    file(APPEND "${_model_d_expected_formats_file}" "${_format}\n")
endforeach()
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
    "-DSYNTH_TEST_REPORTS_FILE=${_model_d_test_reports_file}")
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
            "-DSYNTH_EXPECTED_FORMATS_FILE=${_model_d_expected_formats_file}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyStagedArtifacts.cmake"
    COMMENT "Verifying Model D development artifact staging manifest"
    VERBATIM)

if(SYNTH_BUILD_VALIDATORS)
    if(NOT SYNTH_BUILD_TESTS)
        message(FATAL_ERROR
            "SYNTH_BUILD_VALIDATORS requires SYNTH_BUILD_TESTS for actual-product lifecycle hooks")
    endif()
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
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_SMOKE_EXECUTABLE=$<TARGET_FILE:ModelDActualWrapperSmoke>"
            "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
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

    set(_model_d_standalone_lifecycle_report
        "${CMAKE_BINARY_DIR}/validation/standalone-lifecycle-report.json")
    set(_model_d_standalone_lifecycle_byproducts "")
    foreach(_mode IN ITEMS normal invalid no-device)
        foreach(_repeat RANGE 1 ${SYNTH_VALIDATION_REPEAT_COUNT})
            list(APPEND _model_d_standalone_lifecycle_byproducts
                "${CMAKE_BINARY_DIR}/validation/standalone/${_mode}/repeat-${_repeat}/report.json"
                "${CMAKE_BINARY_DIR}/validation/standalone/${_mode}/repeat-${_repeat}/screenshot.png"
                "${CMAKE_BINARY_DIR}/validation/standalone/${_mode}/repeat-${_repeat}/process.log")
        endforeach()
    endforeach()
    add_custom_command(
        OUTPUT "${_model_d_standalone_lifecycle_report}"
        BYPRODUCTS ${_model_d_standalone_lifecycle_byproducts}
        DEPENDS
            ${_model_d_stage_dependencies}
            Source/StandaloneApp.cpp
            cmake/RunStandaloneLifecycle.cmake
            cmake/VerifyStandaloneLifecycleEvidence.cmake
            cmake/StageDevelopmentArtifacts.cmake
        COMMAND "${CMAKE_COMMAND}" ${_model_d_stage_arguments}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/StageDevelopmentArtifacts.cmake"
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_ROOT}/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_STANDALONE_REPORT_PATH=${_model_d_standalone_lifecycle_report}"
            "-DSYNTH_VALIDATION_DIRECTORY=${CMAKE_BINARY_DIR}/validation"
            "-DSYNTH_REPEAT_COUNT=${SYNTH_VALIDATION_REPEAT_COUNT}"
            "-DSYNTH_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
            "-DSYNTH_ARCHITECTURE=${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_CONFIGURATION=$<CONFIG>"
            "-DSYNTH_PROJECT_VERSION=${PROJECT_VERSION}"
            "-DSYNTH_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunStandaloneLifecycle.cmake"
        COMMENT "Launching the staged standalone in three device modes and fresh processes"
        VERBATIM)
    add_custom_target(ModelDRunStandaloneLifecycle
        DEPENDS "${_model_d_standalone_lifecycle_report}")
    add_dependencies(ModelDRunActualWrapperSmoke ModelDRunStandaloneLifecycle)

    if(APPLE)
        set(_model_d_pluginval_asset "pluginval_macOS.zip")
        set(_model_d_pluginval_archive_sha256
            "3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f")
        set(_model_d_pluginval_executable_relative
            "pluginval.app/Contents/MacOS/pluginval")
    elseif(WIN32)
        set(_model_d_pluginval_asset "pluginval_Windows.zip")
        set(_model_d_pluginval_archive_sha256
            "c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab")
        set(_model_d_pluginval_executable_relative "pluginval.exe")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_model_d_pluginval_asset "pluginval_Linux.zip")
        set(_model_d_pluginval_archive_sha256
            "c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352")
        set(_model_d_pluginval_executable_relative "pluginval")
    else()
        message(FATAL_ERROR
            "pluginval v1.0.4 provisioning is unsupported on ${CMAKE_SYSTEM_NAME}")
    endif()
    set(_model_d_pluginval_url
        "https://github.com/Tracktion/pluginval/releases/download/v1.0.4/${_model_d_pluginval_asset}")
    set(_model_d_pluginval_output_directory
        "${CMAKE_BINARY_DIR}/validation-tools/pluginval/v1.0.4")
    set(_model_d_pluginval_metadata
        "${CMAKE_BINARY_DIR}/validation/pluginval-provision.json")
    set(_model_d_pluginval_stamp
        "${_model_d_pluginval_output_directory}/provision.stamp")
    if(SYNTH_PLUGINVAL_EXECUTABLE)
        set(_model_d_pluginval_resolved_executable "${SYNTH_PLUGINVAL_EXECUTABLE}")
    else()
        set(_model_d_pluginval_resolved_executable
            "${_model_d_pluginval_output_directory}/extracted/${_model_d_pluginval_executable_relative}")
    endif()
    set(_model_d_pluginval_provision_dependencies
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProvisionPluginval.cmake")
    if(SYNTH_PLUGINVAL_EXECUTABLE)
        list(APPEND _model_d_pluginval_provision_dependencies
             "${SYNTH_PLUGINVAL_EXECUTABLE}")
    elseif(SYNTH_PLUGINVAL_ARCHIVE)
        list(APPEND _model_d_pluginval_provision_dependencies
             "${SYNTH_PLUGINVAL_ARCHIVE}")
    endif()
    add_custom_command(
        OUTPUT
            "${_model_d_pluginval_stamp}"
            "${_model_d_pluginval_metadata}"
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DSYNTH_PLUGINVAL_OUTPUT_DIRECTORY=${_model_d_pluginval_output_directory}"
            "-DSYNTH_PLUGINVAL_METADATA_PATH=${_model_d_pluginval_metadata}"
            "-DSYNTH_PLUGINVAL_STAMP_PATH=${_model_d_pluginval_stamp}"
            "-DSYNTH_PLUGINVAL_PLATFORM_ASSET=${_model_d_pluginval_asset}"
            "-DSYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256=${_model_d_pluginval_archive_sha256}"
            "-DSYNTH_PLUGINVAL_DOWNLOAD_URL=${_model_d_pluginval_url}"
            "-DSYNTH_PLUGINVAL_EXECUTABLE_RELATIVE_PATH=${_model_d_pluginval_executable_relative}"
            "-DSYNTH_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
            "-DSYNTH_PLUGINVAL_EXECUTABLE_OVERRIDE=${SYNTH_PLUGINVAL_EXECUTABLE}"
            "-DSYNTH_PLUGINVAL_ARCHIVE_OVERRIDE=${SYNTH_PLUGINVAL_ARCHIVE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProvisionPluginval.cmake"
        DEPENDS ${_model_d_pluginval_provision_dependencies}
        COMMENT "Provisioning pinned pluginval v1.0.4"
        VERBATIM)
    add_custom_target(ModelDProvisionPluginval
        DEPENDS "${_model_d_pluginval_stamp}" "${_model_d_pluginval_metadata}")

    set(_model_d_pluginval_report
        "${CMAKE_BINARY_DIR}/validation/pluginval-run-report.json")
    add_custom_target(ModelDRunPluginval
        DEPENDS
            ModelDProvisionPluginval
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_ROOT}/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_PLUGINVAL_EXECUTABLE=${_model_d_pluginval_resolved_executable}"
            "-DSYNTH_PLUGINVAL_METADATA_PATH=${_model_d_pluginval_metadata}"
            "-DSYNTH_PLUGINVAL_REPORT_PATH=${_model_d_pluginval_report}"
            "-DSYNTH_VALIDATION_DIRECTORY=${CMAKE_BINARY_DIR}/validation"
            "-DSYNTH_REPEAT_COUNT=${SYNTH_VALIDATION_REPEAT_COUNT}"
            "-DSYNTH_SEED=0x4d6f64656c44"
            "-DSYNTH_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
            "-DSYNTH_ARCHITECTURE=${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_CONFIGURATION=$<CONFIG>"
            "-DSYNTH_VST3_VALIDATOR_EXECUTABLE=${SYNTH_VST3_VALIDATOR_EXECUTABLE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunPluginval.cmake"
        COMMENT "Running separate isolated pluginval strictness-10 processes"
        VERBATIM)
    add_dependencies(ModelDRunPluginval ModelDRunActualWrapperSmoke)

    set(_model_d_auval_report
        "${CMAKE_BINARY_DIR}/validation/auval-report.json")
    set(_model_d_auval_log
        "${CMAKE_BINARY_DIR}/validation/auval.log")
    add_custom_target(ModelDRunAuval
        DEPENDS ModelDRunPluginval
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DSYNTH_AUVAL_REPORT_PATH=${_model_d_auval_report}"
            "-DSYNTH_AUVAL_LOG_PATH=${_model_d_auval_log}"
            "-DSYNTH_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
            "-DSYNTH_ARCHITECTURE=${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_CONFIGURATION=$<CONFIG>"
            "-DSYNTH_PRODUCT_CODE=${SYNTH_PRODUCT_CODE}"
            "-DSYNTH_MANUFACTURER_CODE=${SYNTH_MANUFACTURER_CODE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunAuval.cmake"
        COMMENT "Recording non-mutating auval evidence"
        VERBATIM)

    set(_model_d_host_matrix_source
        "${CMAKE_CURRENT_SOURCE_DIR}/validation/required-host-matrix.json")
    set(_model_d_host_matrix_build
        "${CMAKE_BINARY_DIR}/validation/required-host-matrix.json")
    configure_file(
        "${_model_d_host_matrix_source}"
        "${_model_d_host_matrix_build}"
        COPYONLY)

    set(_model_d_validation_reports
        "${_model_d_standalone_lifecycle_report}"
        ${_model_d_standalone_lifecycle_byproducts}
        "${_model_d_wrapper_smoke_report}"
        "${_model_d_wrapper_smoke_log}"
        "${_model_d_pluginval_metadata}"
        "${_model_d_pluginval_report}"
        "${_model_d_auval_report}"
        "${_model_d_auval_log}"
        "${_model_d_host_matrix_build}")
    foreach(_repeat RANGE 1 ${SYNTH_VALIDATION_REPEAT_COUNT})
        list(APPEND _model_d_validation_reports
            "${CMAKE_BINARY_DIR}/validation/pluginval/repeat-${_repeat}/pluginval-report.txt"
            "${CMAKE_BINARY_DIR}/validation/pluginval/repeat-${_repeat}/process.log")
    endforeach()
    if(SYNTH_TEST_REPORTS)
        list(APPEND _model_d_validation_reports ${SYNTH_TEST_REPORTS})
    endif()
    set(_model_d_expected_validation_evidence "")
    foreach(_report IN LISTS _model_d_validation_reports)
        file(RELATIVE_PATH _report_relative "${CMAKE_BINARY_DIR}" "${_report}")
        string(REPLACE "\\" "/" _report_relative "${_report_relative}")
        list(APPEND _model_d_expected_validation_evidence "${_report_relative}")
    endforeach()
    list(REMOVE_DUPLICATES _model_d_expected_validation_evidence)
    list(SORT _model_d_expected_validation_evidence)
    set(_model_d_validation_inventory_directory
        "${CMAKE_BINARY_DIR}/model-d-validation-inventory")
    file(MAKE_DIRECTORY "${_model_d_validation_inventory_directory}")
    set(_model_d_validation_reports_file
        "${_model_d_validation_inventory_directory}/declared-reports.txt")
    set(_model_d_expected_evidence_file
        "${_model_d_validation_inventory_directory}/expected-evidence.txt")
    file(WRITE "${_model_d_validation_reports_file}" "")
    foreach(_report IN LISTS _model_d_validation_reports)
        if(_report MATCHES "[\r\n]")
            message(FATAL_ERROR
                "Validation report paths must not contain line breaks: ${_report}")
        endif()
        file(APPEND "${_model_d_validation_reports_file}" "${_report}\n")
    endforeach()
    file(WRITE "${_model_d_expected_evidence_file}" "")
    foreach(_evidence IN LISTS _model_d_expected_validation_evidence)
        if(_evidence MATCHES "[\r\n]")
            message(FATAL_ERROR
                "Expected validation evidence paths must not contain line breaks: ${_evidence}")
        endif()
        file(APPEND "${_model_d_expected_evidence_file}" "${_evidence}\n")
    endforeach()
    set(_model_d_validation_manifest
        "${SYNTH_STAGE_ROOT}/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}/validation-manifest.json")

    add_custom_target(ModelDFinalizeValidationEvidence
        DEPENDS ModelDRunAuval
        COMMAND "${CMAKE_COMMAND}"
            ${_model_d_stage_arguments}
            "-DSYNTH_TEST_REPORTS_FILE=${_model_d_validation_reports_file}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/StageDevelopmentArtifacts.cmake"
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_ROOT}/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_VALIDATION_MANIFEST_PATH=${_model_d_validation_manifest}"
            "-DSYNTH_PLUGINVAL_METADATA_PATH=${_model_d_pluginval_metadata}"
            "-DSYNTH_PLUGINVAL_REPORT_PATH=${_model_d_pluginval_report}"
            "-DSYNTH_WRAPPER_REPORT_PATH=${_model_d_wrapper_smoke_report}"
            "-DSYNTH_WRAPPER_LOG_PATH=${_model_d_wrapper_smoke_log}"
            "-DSYNTH_AUVAL_REPORT_PATH=${_model_d_auval_report}"
            "-DSYNTH_STANDALONE_REPORT_PATH=${_model_d_standalone_lifecycle_report}"
            "-DSYNTH_HOST_MATRIX_PATH=${_model_d_host_matrix_source}"
            "-DSYNTH_REPEAT_COUNT=${SYNTH_VALIDATION_REPEAT_COUNT}"
            "-DSYNTH_SEED=0x4d6f64656c44"
            "-DSYNTH_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
            "-DSYNTH_ARCHITECTURE=${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_CONFIGURATION=$<CONFIG>"
            "-DSYNTH_PROJECT_VERSION=${PROJECT_VERSION}"
            "-DSYNTH_EXPECTED_EVIDENCE_FILE=${_model_d_expected_evidence_file}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateValidationManifest.cmake"
        COMMENT "Re-staging and finalizing linked validation evidence"
        VERBATIM)

    add_custom_target(ModelDVerifyValidationEvidence
        DEPENDS ModelDFinalizeValidationEvidence
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_ROOT}/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DSYNTH_EXPECTED_FORMATS_FILE=${_model_d_expected_formats_file}"
            -DSYNTH_ALLOW_VALIDATION_MANIFEST=ON
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyStagedArtifacts.cmake"
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
            "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_ROOT}/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_HOST_MATRIX_PATH=${_model_d_host_matrix_source}"
            "-DSYNTH_REPEAT_COUNT=${SYNTH_VALIDATION_REPEAT_COUNT}"
            "-DSYNTH_SEED=0x4d6f64656c44"
            "-DSYNTH_PRODUCT_CODE=${SYNTH_PRODUCT_CODE}"
            "-DSYNTH_MANUFACTURER_CODE=${SYNTH_MANUFACTURER_CODE}"
            "-DSYNTH_PROJECT_VERSION=${PROJECT_VERSION}"
            "-DSYNTH_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
            "-DSYNTH_ARCHITECTURE=${CMAKE_SYSTEM_PROCESSOR}"
            "-DSYNTH_CONFIGURATION=$<CONFIG>"
            "-DSYNTH_EXPECTED_EVIDENCE_FILE=${_model_d_expected_evidence_file}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyValidationManifest.cmake"
        COMMENT "Verifying build and validation manifests and all declared evidence"
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
    target_compile_definitions(ModelDTests PRIVATE
        SYNTH_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

    function(synth_add_model_d_test test_name test_label test_mode)
        add_test(NAME "${test_name}" COMMAND ModelDTests "${test_mode}")
        set_tests_properties("${test_name}" PROPERTIES LABELS "${test_label}")
    endfunction()

    synth_add_model_d_test(ModelDUnitContract unit unit)
    synth_add_model_d_test(ModelDStateSmoke state state)
    synth_add_model_d_test(ModelDLegacyParameterFixtures state fixtures)
    synth_add_model_d_test(ModelDParameterRegistry state registry)
    synth_add_model_d_test(ModelDDspContract dsp dsp)
    synth_add_model_d_test(ModelDMidiSampleZero midi midi)
    synth_add_model_d_test(ModelDRealtimeSmoke realtime realtime)
    synth_add_model_d_test(ModelDHostContract host host)

    add_test(NAME ModelDProductIdentityContracts
        COMMAND "${CMAKE_COMMAND}"
                "-DSYNTH_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DSYNTH_CONTRACT_ROOT=${CMAKE_BINARY_DIR}/product-identity-contracts/$<CONFIG>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/ProductIdentityContracts.cmake")
    set_tests_properties(ModelDProductIdentityContracts PROPERTIES
        LABELS artifact
        TIMEOUT 120)

    if(SYNTH_BUILD_VALIDATORS)
        add_test(NAME ModelDStandaloneLifecycle
            COMMAND "${CMAKE_COMMAND}"
                    "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
                    "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_ROOT}/$<CONFIG>-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
                    "-DSYNTH_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
                    "-DSYNTH_CONFIGURATION=$<CONFIG>"
                    "-DSYNTH_REPEAT_COUNT=${SYNTH_VALIDATION_REPEAT_COUNT}"
                    "-DSYNTH_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
                    "-DSYNTH_ARCHITECTURE=${CMAKE_SYSTEM_PROCESSOR}"
                    "-DSYNTH_PROJECT_VERSION=${PROJECT_VERSION}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/StandaloneLifecycleContracts.cmake")
        set_tests_properties(ModelDStandaloneLifecycle PROPERTIES
            LABELS host
            TIMEOUT 1800)

        add_test(NAME ModelDValidatorPathSafety
            COMMAND "${CMAKE_COMMAND}"
                    "-DSYNTH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
                    "-DSYNTH_CONTRACT_ROOT=${CMAKE_BINARY_DIR}/validator-path-safety-contracts/$<CONFIG>"
                    "-DSYNTH_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
                    "-DSYNTH_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/ValidatorPathSafetyContracts.cmake")
        set_tests_properties(ModelDValidatorPathSafety PROPERTIES
            LABELS artifact
            TIMEOUT 120)
    endif()

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
