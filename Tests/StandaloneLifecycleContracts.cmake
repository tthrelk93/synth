cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_BUILD_ROOT
        SYNTH_STAGE_DIRECTORY
        SYNTH_SOURCE_ROOT
        SYNTH_CONFIGURATION
        SYNTH_REPEAT_COUNT
        SYNTH_SYSTEM_NAME
        SYNTH_ARCHITECTURE
        SYNTH_PROJECT_VERSION)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required for standalone lifecycle contract tests")
    endif()
endforeach()

set(_aggregate_path "${SYNTH_BUILD_ROOT}/validation/standalone-lifecycle-report.json")
set(_verifier "${SYNTH_SOURCE_ROOT}/cmake/VerifyStandaloneLifecycleEvidence.cmake")
set(_runner "${SYNTH_SOURCE_ROOT}/cmake/RunStandaloneLifecycle.cmake")
foreach(_required_file IN ITEMS
        "${SYNTH_SOURCE_ROOT}/Source/StandaloneApp.cpp"
        "${_runner}"
        "${_verifier}")
    if(NOT EXISTS "${_required_file}")
        message(FATAL_ERROR "Standalone lifecycle implementation is missing: ${_required_file}")
    endif()
endforeach()

file(READ "${_runner}" _standalone_runner_source)
file(READ "${_verifier}" _standalone_verifier_source)
if("${_standalone_runner_source}\n${_standalone_verifier_source}" MATCHES
        "legacy_default_settings|_legacy_settings_|<USER_HOME>|<USER_APPDATA>")
    message(FATAL_ERROR
        "Standalone lifecycle validation still probes the real default settings path")
endif()
foreach(_runner_configuration_contract IN ITEMS
        "_synth_json_set_string(_environment configuration"
        "-DSYNTH_CONFIGURATION=\${SYNTH_CONFIGURATION}")
    string(FIND "${_standalone_runner_source}"
           "${_runner_configuration_contract}" _contract_position)
    if(_contract_position LESS 0)
        message(FATAL_ERROR
            "Standalone lifecycle runner is missing configuration provenance: ${_runner_configuration_contract}")
    endif()
endforeach()
foreach(_verifier_configuration_contract IN ITEMS
        "string(JSON _aggregate_configuration GET \"\${_aggregate}\" environment configuration)"
        "_aggregate_configuration STREQUAL SYNTH_CONFIGURATION")
    string(FIND "${_standalone_verifier_source}"
           "${_verifier_configuration_contract}" _contract_position)
    if(_contract_position LESS 0)
        message(FATAL_ERROR
            "Standalone lifecycle verifier is missing configuration provenance: ${_verifier_configuration_contract}")
    endif()
endforeach()
file(READ "${SYNTH_SOURCE_ROOT}/cmake/GenerateValidationManifest.cmake"
     _validation_manifest_generator_source)
file(READ "${SYNTH_SOURCE_ROOT}/cmake/VerifyValidationManifest.cmake"
     _validation_manifest_verifier_source)
foreach(_manifest_configuration_contract IN ITEMS
        "string(JSON _standalone_configuration GET \"\${_standalone}\" environment configuration)"
        "_standalone_configuration STREQUAL SYNTH_CONFIGURATION"
        "_standalone_configuration STREQUAL _build_configuration")
    string(FIND "${_validation_manifest_generator_source}"
           "${_manifest_configuration_contract}" _contract_position)
    if(_contract_position LESS 0)
        message(FATAL_ERROR
            "Validation manifest generation is missing standalone configuration binding: ${_manifest_configuration_contract}")
    endif()
endforeach()
string(FIND "${_validation_manifest_verifier_source}"
       "-DSYNTH_CONFIGURATION=\${SYNTH_CONFIGURATION}" _contract_position)
if(_contract_position LESS 0)
    message(FATAL_ERROR
        "Validation manifest verification does not pass configuration to standalone verification")
endif()

file(READ "${SYNTH_SOURCE_ROOT}/Source/StandaloneApp.cpp" _standalone_source)
file(READ "${SYNTH_SOURCE_ROOT}/Source/PianoKey.h" _piano_key_header)
file(READ "${SYNTH_SOURCE_ROOT}/Tools/ModelDActualWrapperSmoke.cpp" _wrapper_smoke_source)
file(READ "${SYNTH_SOURCE_ROOT}/cmake/RunActualWrapperSmoke.cmake" _wrapper_runner_source)
file(READ "${SYNTH_SOURCE_ROOT}/cmake/RunPluginval.cmake" _pluginval_runner_source)
file(READ "${SYNTH_SOURCE_ROOT}/cmake/GenerateValidationManifest.cmake" _manifest_generator_source)
file(READ "${SYNTH_SOURCE_ROOT}/cmake/VerifyValidationManifest.cmake" _manifest_verifier_source)
file(READ "${SYNTH_SOURCE_ROOT}/.github/workflows/ci.yml" _ci_workflow_source)
foreach(_virtual_display_contract IN ITEMS
        "Start verified Linux virtual display and window manager"
        "x11-utils"
        "openbox"
        "Xvfb \"$display\""
        "DISPLAY=\"$display\" xdpyinfo"
        "DISPLAY=\"$display\" openbox --sm-disable"
        "DISPLAY=\"$display\" xprop -root _NET_SUPPORTING_WM_CHECK"
        "DISPLAY=$display"
        "SYNTH_REUSE_VERIFIED_DISPLAY=1")
    string(FIND "${_ci_workflow_source}"
           "${_virtual_display_contract}" _contract_position)
    if(_contract_position LESS 0)
        message(FATAL_ERROR
            "Hosted CI is missing its verified Linux display contract: ${_virtual_display_contract}")
    endif()
endforeach()
foreach(_display_safety_source IN ITEMS
        _standalone_runner_source
        _wrapper_runner_source
        _pluginval_runner_source)
    string(FIND "${${_display_safety_source}}"
           "^:[0-9]+([.][0-9]+)?$" _display_safety_position)
    if(_display_safety_position LESS 0)
        message(FATAL_ERROR
            "Inherited Linux DISPLAY validation is missing from ${_display_safety_source}")
    endif()
endforeach()
foreach(_wrapper_evidence_contract IN ITEMS
        "virtual_display_provider"
        "DISPLAY=<INHERITED_X11_DISPLAY>"
        "string(JSON _wrapper_report SET")
    string(FIND "${_wrapper_runner_source}"
           "${_wrapper_evidence_contract}" _wrapper_runner_position)
    if(_wrapper_runner_position LESS 0)
        message(FATAL_ERROR
            "Actual-wrapper runner is missing display evidence: ${_wrapper_evidence_contract}")
    endif()
endforeach()
foreach(_wrapper_verification_contract IN ITEMS
        "_wrapper_virtual_display_provider"
        "_wrapper_command"
        "DISPLAY=<INHERITED_X11_DISPLAY>")
    string(FIND "${_manifest_generator_source}\n${_manifest_verifier_source}"
           "${_wrapper_verification_contract}" _wrapper_verifier_position)
    if(_wrapper_verifier_position LESS 0)
        message(FATAL_ERROR
            "Actual-wrapper display evidence is not linked and verified: ${_wrapper_verification_contract}")
    endif()
endforeach()
if(_standalone_source MATCHES "StandalonePluginHolder|StandaloneFilterWindow")
    message(FATAL_ERROR
        "Standalone lifecycle implementation still constructs JUCE's non-deferrable holder/window")
endif()
if(_wrapper_smoke_source MATCHES "windowIsTemporary")
    message(FATAL_ERROR
        "Actual-wrapper editor smoke still requests the unsafe bare-Xvfb borderless path")
endif()
foreach(_wrapper_host_window_contract IN ITEMS
        "class EditorHostWindow final : public juce::DocumentWindow"
        "setContentNonOwned (editor.get(), true)"
        "clearContentComponent()"
        "drainHostWindowEvents")
    string(FIND "${_wrapper_smoke_source}"
           "${_wrapper_host_window_contract}" _wrapper_host_window_position)
    if(_wrapper_host_window_position LESS 0)
        message(FATAL_ERROR
            "Actual-wrapper editor smoke does not use a drained host-container lifecycle: ${_wrapper_host_window_contract}")
    endif()
endforeach()
if(_standalone_source MATCHES "ApplicationProperties|Time::getMillisecond")
    message(FATAL_ERROR
        "Standalone lifecycle still uses default-path settings or wall-clock ordering")
endif()
string(FIND "${_piano_key_header}"
       "bool isKeyPressed = false;" _piano_key_initial_state_position)
if(_piano_key_initial_state_position LESS 0)
    message(FATAL_ERROR
        "PianoKey does not define a deterministic initial released state")
endif()
foreach(_required_contract IN ITEMS
        "--settings"
        "--preset-dir"
        "File::isAbsolutePath (arguments[3])"
        "std::make_unique<PropertiesFile> (lifecycle.settings"
        "setStandaloneLifecycleTestDirectory"
        "private ComponentListener"
        "BorderedComponentBoundsConstrainer"
        "getEditorConstrainer"
        "setBoundsConstrained"
        "editor_constrainer_limits_valid"
        "editor_resize_round_trip"
        "createAudioDeviceTypes"
        "audio_device_discovery_count"
        "status_label_showing")
    string(FIND "${_standalone_source}" "${_required_contract}" _contract_position)
    if(_contract_position LESS 0)
        message(FATAL_ERROR
            "Standalone lifecycle source is missing reviewed contract: ${_required_contract}")
    endif()
endforeach()
foreach(_screenshot_warmup_contract IN ITEMS
        "renderingWarmup"
        "screenshot_render_warmup_valid")
    string(FIND "${_standalone_source}"
           "${_screenshot_warmup_contract}" _warmup_contract_position)
    if(_warmup_contract_position LESS 0)
        message(FATAL_ERROR
            "Standalone screenshot capture is missing renderer warm-up evidence: ${_screenshot_warmup_contract}")
    endif()
endforeach()
foreach(_display_provider_contract IN ITEMS
        "inherited-x11"
        "xvfb-run"
        "virtual_display_provider")
    string(FIND "${_standalone_runner_source}"
           "${_display_provider_contract}" _runner_contract_position)
    string(FIND "${_standalone_verifier_source}"
           "${_display_provider_contract}" _verifier_contract_position)
    if(_runner_contract_position LESS 0 OR _verifier_contract_position LESS 0)
        message(FATAL_ERROR
            "Standalone lifecycle display-provider evidence is incomplete: ${_display_provider_contract}")
    endif()
endforeach()
foreach(_renderer_warmup_contract IN ITEMS
        "renderer-warmup"
        "fresh-process")
    string(FIND "${_standalone_runner_source}"
           "${_renderer_warmup_contract}" _runner_contract_position)
    string(FIND "${_standalone_verifier_source}"
           "${_renderer_warmup_contract}" _verifier_contract_position)
    if(_runner_contract_position LESS 0 OR _verifier_contract_position LESS 0)
        message(FATAL_ERROR
            "Standalone lifecycle renderer warm-up evidence is incomplete: ${_renderer_warmup_contract}")
    endif()
endforeach()
foreach(_windows_path_contract IN ITEMS
        "cmake_path(CONVERT \"\${_reported_settings}\""
        "_reported_settings_normalized"
        "_reported_presets_normalized")
    string(FIND "${_standalone_runner_source}"
           "${_windows_path_contract}" _runner_contract_position)
    if(_runner_contract_position LESS 0)
        message(FATAL_ERROR
            "Standalone lifecycle runner is missing Windows path normalization: ${_windows_path_contract}")
    endif()
endforeach()

file(READ "${SYNTH_SOURCE_ROOT}/Source/PluginProcessor.h" _processor_header)
file(READ "${SYNTH_SOURCE_ROOT}/Source/PluginProcessor.cpp" _processor_source)
if("${_processor_header}\n${_processor_source}" MATCHES
        "FileLogger|my_plugin_log|userDesktopDirectory")
    message(FATAL_ERROR
        "Project processor still writes an unconditional desktop debug log")
endif()

file(READ "${SYNTH_SOURCE_ROOT}/Source/PluginEditor.cpp" _plugin_editor_source)
if(_plugin_editor_source MATCHES "int outputPhonesVolKnobPos\\[2\\];")
    message(FATAL_ERROR
        "Phones Vol control still consumes an uninitialised grid position")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${SYNTH_BUILD_ROOT}"
            --config "${SYNTH_CONFIGURATION}"
            --target ModelDRunStandaloneLifecycle
    RESULT_VARIABLE _build_status
    OUTPUT_VARIABLE _build_stdout
    ERROR_VARIABLE _build_stderr
    ENCODING UTF-8)
if(NOT _build_status EQUAL 0)
    message(FATAL_ERROR
        "Unable to produce standalone lifecycle evidence:\n${_build_stdout}${_build_stderr}")
endif()

set(_verify_command
    "${CMAKE_COMMAND}"
    "-DSYNTH_BUILD_ROOT=${SYNTH_BUILD_ROOT}"
    "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_DIRECTORY}"
    "-DSYNTH_STANDALONE_REPORT_PATH=${_aggregate_path}"
    "-DSYNTH_REPEAT_COUNT=${SYNTH_REPEAT_COUNT}"
    "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
    "-DSYNTH_ARCHITECTURE=${SYNTH_ARCHITECTURE}"
    "-DSYNTH_CONFIGURATION=${SYNTH_CONFIGURATION}"
    "-DSYNTH_PROJECT_VERSION=${SYNTH_PROJECT_VERSION}"
    -P "${_verifier}")

function(_synth_run_verifier_expect_success)
    execute_process(
        COMMAND ${_verify_command}
        RESULT_VARIABLE _status
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
        ENCODING UTF-8)
    if(NOT _status EQUAL 0)
        message(FATAL_ERROR
            "Positive standalone lifecycle verification failed:\n${_stdout}${_stderr}")
    endif()
endfunction()

function(_synth_run_verifier_expect_failure name expected_pattern)
    execute_process(
        COMMAND ${_verify_command}
        RESULT_VARIABLE _status
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
        ENCODING UTF-8)
    set(_combined "${_stdout}${_stderr}")
    if(_status EQUAL 0)
        message(FATAL_ERROR "Negative standalone check unexpectedly passed: ${name}")
    endif()
    if(NOT _combined MATCHES "${expected_pattern}")
        message(FATAL_ERROR
            "Negative standalone check '${name}' failed for the wrong reason:\n${_combined}")
    endif()
endfunction()

function(_synth_run_runner_expect_failure name expected_pattern)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_BUILD_ROOT=${SYNTH_BUILD_ROOT}"
            "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_DIRECTORY}"
            "-DSYNTH_STANDALONE_REPORT_PATH=${_aggregate_path}"
            "-DSYNTH_VALIDATION_DIRECTORY=${SYNTH_BUILD_ROOT}/validation"
            "-DSYNTH_REPEAT_COUNT=${SYNTH_REPEAT_COUNT}"
            "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
            "-DSYNTH_ARCHITECTURE=${SYNTH_ARCHITECTURE}"
            "-DSYNTH_CONFIGURATION=${SYNTH_CONFIGURATION}"
            "-DSYNTH_PROJECT_VERSION=${SYNTH_PROJECT_VERSION}"
            "-DSYNTH_SOURCE_ROOT=${SYNTH_SOURCE_ROOT}"
            -P "${_runner}"
        RESULT_VARIABLE _status
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
        ENCODING UTF-8)
    set(_combined "${_stdout}${_stderr}")
    if(_status EQUAL 0)
        message(FATAL_ERROR "Negative standalone runner check unexpectedly passed: ${name}")
    endif()
    if(NOT _combined MATCHES "${expected_pattern}")
        message(FATAL_ERROR
            "Negative standalone runner check '${name}' failed for the wrong reason:\n${_combined}")
    endif()
endfunction()

function(_synth_run_runner_expect_success)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DSYNTH_BUILD_ROOT=${SYNTH_BUILD_ROOT}"
            "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_DIRECTORY}"
            "-DSYNTH_STANDALONE_REPORT_PATH=${_aggregate_path}"
            "-DSYNTH_VALIDATION_DIRECTORY=${SYNTH_BUILD_ROOT}/validation"
            "-DSYNTH_REPEAT_COUNT=${SYNTH_REPEAT_COUNT}"
            "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
            "-DSYNTH_ARCHITECTURE=${SYNTH_ARCHITECTURE}"
            "-DSYNTH_CONFIGURATION=${SYNTH_CONFIGURATION}"
            "-DSYNTH_PROJECT_VERSION=${SYNTH_PROJECT_VERSION}"
            "-DSYNTH_SOURCE_ROOT=${SYNTH_SOURCE_ROOT}"
            -P "${_runner}"
        RESULT_VARIABLE _status
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
        ENCODING UTF-8)
    if(NOT _status EQUAL 0)
        message(FATAL_ERROR
            "Fresh standalone runner invocation failed:\n${_stdout}${_stderr}")
    endif()
    if(EXISTS "${SYNTH_BUILD_ROOT}/validation/standalone/runtime-isolation"
       OR IS_SYMLINK "${SYNTH_BUILD_ROOT}/validation/standalone/runtime-isolation")
        message(FATAL_ERROR "Fresh standalone runner left runtime-isolation behind")
    endif()
endfunction()

function(_synth_set_json_string output_variable json)
    set(_arguments ${ARGN})
    list(POP_BACK _arguments _value)
    string(REPLACE "\\" "\\\\" _value "${_value}")
    string(REPLACE "\"" "\\\"" _value "${_value}")
    set(_quoted "\"${_value}\"")
    string(JSON _updated SET "${json}" ${_arguments} "${_quoted}")
    set(${output_variable} "${_updated}" PARENT_SCOPE)
endfunction()

_synth_run_verifier_expect_success()
file(READ "${_aggregate_path}" _aggregate_original)

# Raw relative CLI tokens must be rejected before juce::File can resolve them
# against the working directory or any platform default.
string(JSON _standalone_executable_relative GET "${_aggregate_original}" executable relative_path)
set(_standalone_executable "${SYNTH_STAGE_DIRECTORY}/${_standalone_executable_relative}")
if(NOT EXISTS "${_standalone_executable}" OR IS_DIRECTORY "${_standalone_executable}")
    message(FATAL_ERROR "Relative-path negative cannot find the staged standalone executable")
endif()
set(_relative_cli_root "${SYNTH_BUILD_ROOT}/validation/standalone-relative-cli-negative")
if(IS_SYMLINK "${_relative_cli_root}")
    message(FATAL_ERROR "Refusing to use a symlinked relative-path negative root")
endif()
if(EXISTS "${_relative_cli_root}")
    file(REMOVE_RECURSE "${_relative_cli_root}")
endif()
file(MAKE_DIRECTORY "${_relative_cli_root}")
set(_relative_cli_command "${_standalone_executable}"
    --synth-lifecycle-test no-device
    --report relative-report.json
    --screenshot relative-screenshot.png
    --settings relative-settings/MiniMoog.settings
    --preset-dir relative-presets)
if(SYNTH_SYSTEM_NAME STREQUAL "Linux")
    if("$ENV{SYNTH_REUSE_VERIFIED_DISPLAY}" STREQUAL "1")
        if(NOT "$ENV{DISPLAY}" MATCHES "^:[0-9]+([.][0-9]+)?$")
            message(FATAL_ERROR
                "verified Linux display reuse requires a safe local DISPLAY value")
        endif()
    else()
        list(PREPEND _relative_cli_command xvfb-run -a)
    endif()
endif()
execute_process(
    COMMAND ${_relative_cli_command}
    WORKING_DIRECTORY "${_relative_cli_root}"
    TIMEOUT 30
    RESULT_VARIABLE _relative_cli_status
    OUTPUT_VARIABLE _relative_cli_stdout
    ERROR_VARIABLE _relative_cli_stderr
    ENCODING UTF-8)
if(_relative_cli_status STREQUAL "0")
    message(FATAL_ERROR "Standalone accepted relative lifecycle output/state paths")
endif()
foreach(_unexpected_relative_output IN ITEMS
        relative-report.json relative-screenshot.png relative-settings relative-presets)
    if(EXISTS "${_relative_cli_root}/${_unexpected_relative_output}"
       OR IS_SYMLINK "${_relative_cli_root}/${_unexpected_relative_output}")
        message(FATAL_ERROR
            "Rejected relative lifecycle CLI created output: ${_unexpected_relative_output}")
    endif()
endforeach()
file(REMOVE_RECURSE "${_relative_cli_root}")

# Missing report and screenshot evidence must be rejected and restored exactly.
set(_first_report "${SYNTH_BUILD_ROOT}/validation/standalone/normal/repeat-1/report.json")
set(_first_screenshot "${SYNTH_BUILD_ROOT}/validation/standalone/normal/repeat-1/screenshot.png")
file(RENAME "${_first_report}" "${_first_report}.negative-backup")
_synth_run_verifier_expect_failure("missing report" "Standalone report evidence is missing")
file(RENAME "${_first_report}.negative-backup" "${_first_report}")
file(RENAME "${_first_screenshot}" "${_first_screenshot}.negative-backup")
_synth_run_verifier_expect_failure("missing screenshot" "Standalone screenshot evidence is missing")
file(RENAME "${_first_screenshot}.negative-backup" "${_first_screenshot}")

# Corrupt JSON is rejected after its evidence hash is kept internally consistent.
file(READ "${_first_report}" _first_report_original)
file(WRITE "${_first_report}" "{")
file(SHA256 "${_first_report}" _corrupt_report_sha256)
_synth_set_json_string(_aggregate_mutated "${_aggregate_original}"
                       runs 0 report_sha256 "${_corrupt_report_sha256}")
file(WRITE "${_aggregate_path}" "${_aggregate_mutated}\n")
_synth_run_verifier_expect_failure("corrupt app report" "invalid JSON")
file(WRITE "${_first_report}" "${_first_report_original}")
file(WRITE "${_aggregate_path}" "${_aggregate_original}")

# Explicit settings and preset paths are part of the evidence contract.
file(READ "${_first_report}" _report_mutated)
_synth_set_json_string(_report_mutated "${_report_mutated}"
                       settings path "validation/standalone/outside-settings")
file(WRITE "${_first_report}" "${_report_mutated}\n")
file(SHA256 "${_first_report}" _mutated_report_sha256)
_synth_set_json_string(_aggregate_mutated "${_aggregate_original}"
                       runs 0 report_sha256 "${_mutated_report_sha256}")
file(WRITE "${_aggregate_path}" "${_aggregate_mutated}\n")
_synth_run_verifier_expect_failure("settings path mismatch" "isolated settings/preset paths")
file(WRITE "${_first_report}" "${_first_report_original}")
file(WRITE "${_aggregate_path}" "${_aggregate_original}")

# No-device evidence must prove that the discovery seam was never called.
set(_no_device_report "${SYNTH_BUILD_ROOT}/validation/standalone/no-device/repeat-1/report.json")
file(READ "${_no_device_report}" _no_device_report_original)
string(JSON _no_device_report_mutated SET "${_no_device_report_original}"
       discovery audio_device_discovery_count 1)
file(WRITE "${_no_device_report}" "${_no_device_report_mutated}\n")
file(SHA256 "${_no_device_report}" _no_device_report_sha256)
math(EXPR _no_device_run_index "2 * ${SYNTH_REPEAT_COUNT}")
_synth_set_json_string(_aggregate_mutated "${_aggregate_original}"
                       runs ${_no_device_run_index} report_sha256 "${_no_device_report_sha256}")
file(WRITE "${_aggregate_path}" "${_aggregate_mutated}\n")
_synth_run_verifier_expect_failure("forbidden no-device discovery" "forbidden audio/MIDI discovery")
file(WRITE "${_no_device_report}" "${_no_device_report_original}")
file(WRITE "${_aggregate_path}" "${_aggregate_original}")

# Aggregate executable identity, run mode, and aggregate status are strict.
_synth_set_json_string(_aggregate_mutated "${_aggregate_original}"
                       executable sha256
                       "0000000000000000000000000000000000000000000000000000000000000000")
file(WRITE "${_aggregate_path}" "${_aggregate_mutated}\n")
_synth_run_verifier_expect_failure("wrong executable hash" "executable identity/hash differs")
_synth_set_json_string(_aggregate_mutated "${_aggregate_original}" runs 0 mode "unsupported")
file(WRITE "${_aggregate_path}" "${_aggregate_mutated}\n")
_synth_run_verifier_expect_failure("invalid mode" "mode/repeat/status is inconsistent")
_synth_set_json_string(_aggregate_mutated "${_aggregate_original}" status "fail")
file(WRITE "${_aggregate_path}" "${_aggregate_mutated}\n")
_synth_run_verifier_expect_failure("aggregate status inconsistency" "header/status is inconsistent")
_synth_set_json_string(_aggregate_mutated "${_aggregate_original}"
                       environment configuration "WrongConfiguration")
file(WRITE "${_aggregate_path}" "${_aggregate_mutated}\n")
_synth_run_verifier_expect_failure("wrong configuration" "header/status is inconsistent")
file(WRITE "${_aggregate_path}" "${_aggregate_original}")

# A failed actual-window assertion remains fatal even if its hash is updated.
file(READ "${_first_report}" _report_mutated)
string(JSON _report_mutated SET "${_report_mutated}" assertions top_level_visible false)
file(WRITE "${_first_report}" "${_report_mutated}\n")
file(SHA256 "${_first_report}" _mutated_report_sha256)
_synth_set_json_string(_aggregate_mutated "${_aggregate_original}"
                       runs 0 report_sha256 "${_mutated_report_sha256}")
file(WRITE "${_aggregate_path}" "${_aggregate_mutated}\n")
_synth_run_verifier_expect_failure("failed window assertion" "top_level_visible")
file(WRITE "${_first_report}" "${_first_report_original}")
file(WRITE "${_aggregate_path}" "${_aggregate_original}")

# Resize propagation is required independently of the other window assertions.
file(READ "${_first_report}" _report_mutated)
string(JSON _report_mutated SET "${_report_mutated}"
       assertions editor_resize_round_trip false)
file(WRITE "${_first_report}" "${_report_mutated}\n")
file(SHA256 "${_first_report}" _mutated_report_sha256)
_synth_set_json_string(_aggregate_mutated "${_aggregate_original}"
                       runs 0 report_sha256 "${_mutated_report_sha256}")
file(WRITE "${_aggregate_path}" "${_aggregate_mutated}\n")
_synth_run_verifier_expect_failure("failed resize assertion" "editor_resize_round_trip")
file(WRITE "${_first_report}" "${_first_report_original}")
file(WRITE "${_aggregate_path}" "${_aggregate_original}")

# A forged resize relationship is rejected even when every assertion says true.
file(READ "${_first_report}" _report_mutated)
string(JSON _resized_window_width GET "${_report_mutated}" resize resized_window_width)
math(EXPR _forged_resized_window_width "${_resized_window_width} + 1")
string(JSON _report_mutated SET "${_report_mutated}"
       resize resized_window_width ${_forged_resized_window_width})
file(WRITE "${_first_report}" "${_report_mutated}\n")
file(SHA256 "${_first_report}" _mutated_report_sha256)
_synth_set_json_string(_aggregate_mutated "${_aggregate_original}"
                       runs 0 report_sha256 "${_mutated_report_sha256}")
file(WRITE "${_aggregate_path}" "${_aggregate_mutated}\n")
_synth_run_verifier_expect_failure("forged resize relationship" "resize round-trip is inconsistent")
file(WRITE "${_first_report}" "${_first_report_original}")
file(WRITE "${_aggregate_path}" "${_aggregate_original}")

# Zero repeats are rejected at the verifier boundary.
set(_verify_command_saved ${_verify_command})
set(_verify_command
    "${CMAKE_COMMAND}"
    "-DSYNTH_BUILD_ROOT=${SYNTH_BUILD_ROOT}"
    "-DSYNTH_STAGE_DIRECTORY=${SYNTH_STAGE_DIRECTORY}"
    "-DSYNTH_STANDALONE_REPORT_PATH=${_aggregate_path}"
    "-DSYNTH_REPEAT_COUNT=0"
    "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
    "-DSYNTH_ARCHITECTURE=${SYNTH_ARCHITECTURE}"
    "-DSYNTH_CONFIGURATION=${SYNTH_CONFIGURATION}"
    "-DSYNTH_PROJECT_VERSION=${SYNTH_PROJECT_VERSION}"
    -P "${_verifier}")
_synth_run_verifier_expect_failure("zero repeat count" "positive integer")
set(_verify_command ${_verify_command_saved})

# The staged manifest cannot point outside the stage.
set(_build_manifest_path "${SYNTH_STAGE_DIRECTORY}/build-manifest.json")
file(READ "${_build_manifest_path}" _build_manifest_original)
set(_standalone_index -1)
string(JSON _product_count LENGTH "${_build_manifest_original}" products)
math(EXPR _last_product "${_product_count} - 1")
foreach(_product_index RANGE 0 ${_last_product})
    string(JSON _format GET "${_build_manifest_original}" products ${_product_index} format)
    if(_format STREQUAL "Standalone")
        set(_standalone_index ${_product_index})
    endif()
endforeach()
if(_standalone_index LESS 0)
    message(FATAL_ERROR "Negative fixture could not find the Standalone product")
endif()
_synth_set_json_string(_build_manifest_mutated "${_build_manifest_original}"
                       products ${_standalone_index} files 0 path "../escape")
file(WRITE "${_build_manifest_path}" "${_build_manifest_mutated}\n")
_synth_run_runner_expect_failure("manifest path traversal" "unsafe relative path")
file(WRITE "${_build_manifest_path}" "${_build_manifest_original}")

# Cleaning refuses symlinked or unmarked evidence roots.
set(_standalone_root "${SYNTH_BUILD_ROOT}/validation/standalone")
set(_standalone_backup "${SYNTH_BUILD_ROOT}/validation/standalone.negative-backup")
set(_marker "${SYNTH_BUILD_ROOT}/validation-ownership/standalone-evidence-root.marker")
set(_symlink_target "${SYNTH_BUILD_ROOT}/standalone-negative-symlink-target")
file(MAKE_DIRECTORY "${_symlink_target}")
file(RENAME "${_standalone_root}" "${_standalone_backup}")
file(CREATE_LINK "${_symlink_target}" "${_standalone_root}" SYMBOLIC RESULT _link_result)
if(_link_result STREQUAL "0")
    _synth_run_runner_expect_failure(
        "symlinked evidence root" "standalone evidence root has a symlink ancestor")
    file(REMOVE "${_standalone_root}")
else()
    message(STATUS
        "Standalone symlink-root negative not run: symbolic-link creation is unavailable (${_link_result})")
    if(EXISTS "${_standalone_root}" OR IS_SYMLINK "${_standalone_root}")
        file(REMOVE_RECURSE "${_standalone_root}")
    endif()
endif()
file(RENAME "${_standalone_backup}" "${_standalone_root}")
file(REMOVE_RECURSE "${_symlink_target}")

file(READ "${_marker}" _marker_original)
file(RENAME "${_standalone_root}" "${_standalone_backup}")
file(REMOVE "${_marker}")
file(MAKE_DIRECTORY "${_standalone_root}")
_synth_run_runner_expect_failure("unmarked evidence root" "unmarked standalone evidence root")
file(REMOVE_RECURSE "${_standalone_root}")
file(RENAME "${_standalone_backup}" "${_standalone_root}")
file(WRITE "${_marker}" "${_marker_original}")

_synth_run_verifier_expect_success()

# Two independent 3x3 executions must produce an identical aggregate and leave
# no settings/preset isolation tree behind.
_synth_run_runner_expect_success()
file(READ "${_aggregate_path}" _fresh_aggregate_one)
_synth_run_runner_expect_success()
file(READ "${_aggregate_path}" _fresh_aggregate_two)
if(NOT _fresh_aggregate_one STREQUAL _fresh_aggregate_two)
    message(FATAL_ERROR
        "Fresh standalone lifecycle runner aggregates are not byte-identical")
endif()
_synth_run_verifier_expect_success()
message(STATUS "Standalone lifecycle positive and negative contracts passed")
