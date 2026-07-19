cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_BUILD_ROOT
        SYNTH_STAGE_DIRECTORY
        SYNTH_PLUGINVAL_EXECUTABLE
        SYNTH_PLUGINVAL_METADATA_PATH
        SYNTH_PLUGINVAL_REPORT_PATH
        SYNTH_VALIDATION_DIRECTORY
        SYNTH_REPEAT_COUNT
        SYNTH_SEED
        SYNTH_SYSTEM_NAME
        SYNTH_ARCHITECTURE
        SYNTH_CONFIGURATION)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required to run pluginval")
    endif()
endforeach()
if(NOT SYNTH_REPEAT_COUNT MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "SYNTH_REPEAT_COUNT must be a positive integer")
endif()

function(_synth_json_quote output_variable input_value)
    set(_escaped "${input_value}")
    string(REPLACE "\\" "\\\\" _escaped "${_escaped}")
    string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
    string(REPLACE "\n" "\\n" _escaped "${_escaped}")
    string(REPLACE "\r" "\\r" _escaped "${_escaped}")
    set(${output_variable} "\"${_escaped}\"" PARENT_SCOPE)
endfunction()

set(_pluginval_environment_variables
    BLOCK_SIZES
    DATA_FILE
    DISABLED_TESTS
    HELP
    OUTPUT_DIR
    OUTPUT_FILENAME
    RANDOM_SEED
    RANDOMISE
    REPEAT
    RUN_TESTS
    SAMPLE_RATES
    SKIP_GUI_TESTS
    STRICTNESS_LEVEL
    STRICTNESSLEVEL
    TIMEOUT_MS
    TIMEOUTMS
    VALIDATE
    VALIDATE_IN_PROCESS
    VERBOSE
    VERSION
    VST3VALIDATOR)
set(_sanitized_environment_command "${CMAKE_COMMAND}" -E env)
foreach(_environment_variable IN LISTS _pluginval_environment_variables)
    list(APPEND _sanitized_environment_command "--unset=${_environment_variable}")
endforeach()
list(APPEND _sanitized_environment_command --)
list(JOIN _pluginval_environment_variables "," _unset_environment_record)

function(_synth_json_set_string json_variable key value)
    _synth_json_quote(_quoted "${value}")
    string(JSON _updated SET "${${json_variable}}" "${key}" "${_quoted}")
    set(${json_variable} "${_updated}" PARENT_SCOPE)
endfunction()

function(_synth_safe_existing_path output_variable root_directory relative_path description)
    string(REPLACE "\\" "/" _relative_path "${relative_path}")
    if(_relative_path STREQUAL ""
       OR _relative_path MATCHES "^/"
       OR _relative_path MATCHES "^[A-Za-z]:"
       OR _relative_path MATCHES "(^|/)[.][.](/|$)")
        message(FATAL_ERROR "${description} has an unsafe path: ${relative_path}")
    endif()
    file(REAL_PATH "${root_directory}" _root_real)
    if(NOT EXISTS "${_root_real}/${_relative_path}")
        message(FATAL_ERROR "${description} is missing: ${_relative_path}")
    endif()
    file(REAL_PATH "${_root_real}/${_relative_path}" _candidate_real)
    file(RELATIVE_PATH _from_root "${_root_real}" "${_candidate_real}")
    string(REPLACE "\\" "/" _from_root "${_from_root}")
    if(_from_root MATCHES "(^|/)[.][.](/|$)" OR IS_ABSOLUTE "${_from_root}")
        message(FATAL_ERROR "${description} escapes its trusted root: ${relative_path}")
    endif()
    set(${output_variable} "${_candidate_real}" PARENT_SCOPE)
endfunction()

if(NOT EXISTS "${SYNTH_PLUGINVAL_EXECUTABLE}")
    message(FATAL_ERROR "pluginval executable is missing: ${SYNTH_PLUGINVAL_EXECUTABLE}")
endif()
if(NOT EXISTS "${SYNTH_PLUGINVAL_METADATA_PATH}")
    message(FATAL_ERROR "pluginval provisioning metadata is missing: ${SYNTH_PLUGINVAL_METADATA_PATH}")
endif()
file(READ "${SYNTH_PLUGINVAL_METADATA_PATH}" _provision)
string(JSON _provision_type ERROR_VARIABLE _provision_error TYPE "${_provision}")
if(_provision_error OR NOT _provision_type STREQUAL "OBJECT")
    message(FATAL_ERROR "pluginval provisioning metadata is invalid: ${_provision_error}")
endif()
string(JSON _tool_version GET "${_provision}" version)
string(JSON _declared_executable_sha256 GET "${_provision}" executable_sha256)
if(NOT _tool_version STREQUAL "1.0.4")
    message(FATAL_ERROR "pluginval provisioning metadata has an unsupported version: ${_tool_version}")
endif()
file(SHA256 "${SYNTH_PLUGINVAL_EXECUTABLE}" _actual_executable_sha256)
if(NOT _actual_executable_sha256 STREQUAL _declared_executable_sha256)
    message(FATAL_ERROR "pluginval executable hash does not match provisioning metadata")
endif()
execute_process(
    COMMAND ${_sanitized_environment_command}
            "${SYNTH_PLUGINVAL_EXECUTABLE}" --version
    RESULT_VARIABLE _version_status
    OUTPUT_VARIABLE _version_output
    ERROR_VARIABLE _version_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    TIMEOUT 15)
if(NOT "${_version_status}" MATCHES "^[0-9]+$"
   OR NOT "${_version_status}" STREQUAL "0"
   OR NOT _version_output STREQUAL "pluginval - 1.0.4")
    message(FATAL_ERROR
        "pluginval version check failed before validation: '${_version_output}' ${_version_error}")
endif()

set(_build_manifest_path "${SYNTH_STAGE_DIRECTORY}/build-manifest.json")
if(NOT EXISTS "${_build_manifest_path}")
    message(FATAL_ERROR "staged build manifest is missing: ${_build_manifest_path}")
endif()
file(READ "${_build_manifest_path}" _build_manifest)
string(JSON _product_count ERROR_VARIABLE _product_error LENGTH "${_build_manifest}" products)
if(_product_error)
    message(FATAL_ERROR "could not parse staged build manifest products: ${_product_error}")
endif()
set(_vst3_count 0)
if(_product_count GREATER 0)
    math(EXPR _last_product "${_product_count} - 1")
    foreach(_product_index RANGE 0 ${_last_product})
        string(JSON _format GET "${_build_manifest}" products ${_product_index} format)
        if(_format STREQUAL "VST3")
            math(EXPR _vst3_count "${_vst3_count} + 1")
            string(JSON _vst3_relative_path GET "${_build_manifest}" products ${_product_index} relative_path)
            string(JSON _vst3_aggregate_sha256 GET "${_build_manifest}" products ${_product_index} aggregate_sha256)
        endif()
    endforeach()
endif()
if(NOT _vst3_count EQUAL 1)
    message(FATAL_ERROR "build manifest must declare exactly one VST3 product")
endif()
set(_staged_vst3_lexical "${SYNTH_STAGE_DIRECTORY}/${_vst3_relative_path}")
_synth_safe_existing_path(_staged_vst3 "${SYNTH_STAGE_DIRECTORY}"
                          "${_vst3_relative_path}" "staged VST3 product")

file(REAL_PATH "${SYNTH_BUILD_ROOT}" _build_root)
cmake_path(ABSOLUTE_PATH SYNTH_BUILD_ROOT NORMALIZE OUTPUT_VARIABLE _build_root_lexical)
file(TO_NATIVE_PATH "${_build_root}" _build_root_native)
file(TO_NATIVE_PATH "${_build_root_lexical}" _build_root_lexical_native)
file(REAL_PATH "${SYNTH_VALIDATION_DIRECTORY}" _validation_directory)
set(_expected_validation_directory "${_build_root}/validation")
cmake_path(NORMAL_PATH _expected_validation_directory
           OUTPUT_VARIABLE _expected_validation_directory)
if(NOT _validation_directory STREQUAL _expected_validation_directory)
    message(FATAL_ERROR
        "SYNTH_VALIDATION_DIRECTORY must be the fixed build-owned validation directory")
endif()
cmake_path(ABSOLUTE_PATH SYNTH_PLUGINVAL_REPORT_PATH NORMALIZE
           OUTPUT_VARIABLE _pluginval_report_path)
if(NOT _pluginval_report_path STREQUAL "${_build_root_lexical}/validation/pluginval-run-report.json")
    message(FATAL_ERROR "pluginval report path must be the fixed build-owned report path")
endif()
file(RELATIVE_PATH _staged_vst3_from_build "${_build_root_lexical}" "${_staged_vst3_lexical}")
string(REPLACE "\\" "/" _staged_vst3_from_build "${_staged_vst3_from_build}")
if(IS_ABSOLUTE "${_staged_vst3_from_build}"
   OR _staged_vst3_from_build MATCHES "(^|/)[.][.](/|$)")
    message(FATAL_ERROR "staged VST3 product is not build-owned")
endif()
set(_pluginval_evidence_root "${_validation_directory}/pluginval")
set(_pluginval_evidence_marker
    "${_build_root_lexical}/validation-ownership/pluginval-evidence-root.marker")
if(EXISTS "${_pluginval_evidence_root}" OR IS_SYMLINK "${_pluginval_evidence_root}")
    if(IS_SYMLINK "${_pluginval_evidence_root}")
        message(FATAL_ERROR "refusing to clean a symlinked pluginval evidence root")
    endif()
    file(REAL_PATH "${_pluginval_evidence_root}" _existing_evidence_root_real)
    cmake_path(IS_PREFIX _validation_directory "${_existing_evidence_root_real}"
               NORMALIZE _existing_evidence_is_validation_owned)
    if(NOT _existing_evidence_is_validation_owned
       OR _existing_evidence_root_real STREQUAL _validation_directory)
        message(FATAL_ERROR "refusing to clean pluginval evidence outside validation")
    endif()
    if(NOT EXISTS "${_pluginval_evidence_marker}")
        message(FATAL_ERROR
            "refusing to clean an unmarked pluginval evidence root: ${_pluginval_evidence_root}")
    endif()
    file(READ "${_pluginval_evidence_marker}" _evidence_marker_contents)
    if(NOT _evidence_marker_contents STREQUAL "model-d-pluginval-evidence-root-v1\n")
        message(FATAL_ERROR "pluginval evidence-root ownership marker is invalid")
    endif()
    file(REMOVE_RECURSE "${_pluginval_evidence_root}")
endif()
file(MAKE_DIRECTORY "${_pluginval_evidence_root}")
get_filename_component(_pluginval_evidence_marker_directory
                       "${_pluginval_evidence_marker}" DIRECTORY)
file(MAKE_DIRECTORY "${_pluginval_evidence_marker_directory}")
file(WRITE "${_pluginval_evidence_marker}"
     "model-d-pluginval-evidence-root-v1\n")
set(_command_options
    --validate
    "${_vst3_relative_path}"
    --strictness-level 10
    --random-seed "${SYNTH_SEED}"
    --timeout-ms 60000)
set(_vst3_validator_status "not-run")
set(_vst3_validator_reason "No Steinberg VST3 SDK validator executable was configured")
set(_vst3_validator_sha256 "")
if(DEFINED SYNTH_VST3_VALIDATOR_EXECUTABLE
   AND NOT "${SYNTH_VST3_VALIDATOR_EXECUTABLE}" STREQUAL "")
    if(NOT EXISTS "${SYNTH_VST3_VALIDATOR_EXECUTABLE}")
        message(FATAL_ERROR
            "Configured Steinberg VST3 SDK validator is missing: ${SYNTH_VST3_VALIDATOR_EXECUTABLE}")
    endif()
    file(SHA256 "${SYNTH_VST3_VALIDATOR_EXECUTABLE}" _vst3_validator_sha256)
    list(APPEND _command_options --vst3validator "configured-vst3-validator")
    set(_vst3_validator_status "in-progress")
    set(_vst3_validator_reason "Configured validator has not yet completed")
endif()

set(_prefix "")
if(SYNTH_SYSTEM_NAME STREQUAL "Linux")
    find_program(_xvfb_run NAMES xvfb-run)
    if(NOT _xvfb_run)
        message(FATAL_ERROR "Linux GUI validation requires xvfb-run")
    endif()
    set(_prefix "${_xvfb_run}" -a)
endif()

set(_repeats_json "[]")
set(_evidence_paths "")
foreach(_repeat RANGE 1 ${SYNTH_REPEAT_COUNT})
    set(_repeat_directory "${_validation_directory}/pluginval/repeat-${_repeat}")
    set(_repeat_directory_relative "validation/pluginval/repeat-${_repeat}")
    file(REMOVE_RECURSE "${_repeat_directory}")
    file(MAKE_DIRECTORY "${_repeat_directory}")
    set(_validator_report_name "pluginval-report.txt")
    set(_validator_report_path "${_repeat_directory}/${_validator_report_name}")
    set(_process_log_path "${_repeat_directory}/process.log")
    set(_command
        ${_sanitized_environment_command}
        ${_prefix}
        "${SYNTH_PLUGINVAL_EXECUTABLE}"
        --validate "${_staged_vst3_from_build}"
        --strictness-level 10
        --random-seed "${SYNTH_SEED}"
        --timeout-ms 60000
        --output-dir "${_repeat_directory_relative}"
        --output-filename "${_validator_report_name}")
    set(_recorded_command cmake -E env)
    foreach(_environment_variable IN LISTS _pluginval_environment_variables)
        list(APPEND _recorded_command "--unset=${_environment_variable}")
    endforeach()
    list(APPEND _recorded_command --)
    if(SYNTH_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND _recorded_command xvfb-run -a)
    endif()
    list(APPEND _recorded_command
        pluginval
        --validate "${_staged_vst3_from_build}"
        --strictness-level 10
        --random-seed "${SYNTH_SEED}"
        --timeout-ms 60000
        --output-dir "${_repeat_directory_relative}"
        --output-filename "${_validator_report_name}")
    if(DEFINED SYNTH_VST3_VALIDATOR_EXECUTABLE
       AND NOT "${SYNTH_VST3_VALIDATOR_EXECUTABLE}" STREQUAL "")
        list(APPEND _command --vst3validator "${SYNTH_VST3_VALIDATOR_EXECUTABLE}")
        list(APPEND _recorded_command --vst3validator configured-vst3-validator)
    endif()
    list(JOIN _recorded_command " | " _command_record)

    execute_process(
        COMMAND ${_command}
        RESULT_VARIABLE _validator_status
        OUTPUT_VARIABLE _validator_output
        ERROR_VARIABLE _validator_error
        TIMEOUT 900
        WORKING_DIRECTORY "${_build_root_lexical}")
    string(REPLACE "${_build_root_native}" "<BUILD_ROOT>" _validator_output "${_validator_output}")
    string(REPLACE "${_build_root_native}" "<BUILD_ROOT>" _validator_error "${_validator_error}")
    string(REPLACE "${_build_root_lexical_native}" "<BUILD_ROOT>" _validator_output "${_validator_output}")
    string(REPLACE "${_build_root_lexical_native}" "<BUILD_ROOT>" _validator_error "${_validator_error}")
    string(REPLACE "${_build_root}" "<BUILD_ROOT>" _validator_output "${_validator_output}")
    string(REPLACE "${_build_root}" "<BUILD_ROOT>" _validator_error "${_validator_error}")
    string(REPLACE "${_build_root_lexical}" "<BUILD_ROOT>" _validator_output "${_validator_output}")
    string(REPLACE "${_build_root_lexical}" "<BUILD_ROOT>" _validator_error "${_validator_error}")
    file(WRITE "${_process_log_path}"
        "tool=pluginval - 1.0.4\nrepeat=${_repeat}\nisolated_process=true\ngui_tests=enabled\nenvironment_sanitized=true\nunset_environment_variables=${_unset_environment_record}\nconfiguration=${SYNTH_CONFIGURATION}\nos=${SYNTH_SYSTEM_NAME}\narchitecture=${SYNTH_ARCHITECTURE}\nstrictness_level=10\nrandom_seed=${SYNTH_SEED}\ntimeout_ms=60000\nvalidate=${_vst3_relative_path}\ncommand_tokens=${_command_record}\nstdout_begin\n${_validator_output}stdout_end\nstderr_begin\n${_validator_error}stderr_end\nexit_code=${_validator_status}\n")
    if(NOT "${_validator_status}" MATCHES "^[0-9]+$"
       OR NOT "${_validator_status}" STREQUAL "0")
        message(FATAL_ERROR
            "pluginval repeat ${_repeat} failed with exit ${_validator_status}; see ${_process_log_path}")
    endif()
    if(NOT EXISTS "${_validator_report_path}")
        message(FATAL_ERROR
            "pluginval repeat ${_repeat} did not create ${_validator_report_path}")
    endif()
    file(READ "${_validator_report_path}" _validator_report_contents)
    foreach(_required_gui_test IN ITEMS
            "Starting tests in: pluginval / Editor..."
            "Completed tests in pluginval / Editor"
            "Starting tests in: pluginval / Open editor whilst processing..."
            "Completed tests in pluginval / Open editor whilst processing"
            "Starting tests in: pluginval / Editor Automation..."
            "Completed tests in pluginval / Editor Automation")
        string(FIND "${_validator_report_contents}" "${_required_gui_test}"
               _required_gui_test_position)
        if(_required_gui_test_position EQUAL -1)
            message(FATAL_ERROR
                "pluginval repeat ${_repeat} omitted required GUI test evidence: ${_required_gui_test}")
        endif()
    endforeach()
    string(REPLACE "${_build_root_native}" "<BUILD_ROOT>"
                   _validator_report_contents "${_validator_report_contents}")
    string(REPLACE "${_build_root_lexical_native}" "<BUILD_ROOT>"
                   _validator_report_contents "${_validator_report_contents}")
    string(REPLACE "${_build_root}" "<BUILD_ROOT>"
                   _validator_report_contents "${_validator_report_contents}")
    string(REPLACE "${_build_root_lexical}" "<BUILD_ROOT>"
                   _validator_report_contents "${_validator_report_contents}")
    file(WRITE "${_validator_report_path}" "${_validator_report_contents}")
    file(SHA256 "${_validator_report_path}" _validator_report_sha256)
    file(SHA256 "${_process_log_path}" _process_log_sha256)
    file(RELATIVE_PATH _validator_report_relative "${_build_root}" "${_validator_report_path}")
    file(RELATIVE_PATH _process_log_relative "${_build_root}" "${_process_log_path}")
    string(REPLACE "\\" "/" _validator_report_relative "${_validator_report_relative}")
    string(REPLACE "\\" "/" _process_log_relative "${_process_log_relative}")
    list(APPEND _evidence_paths "${_validator_report_relative}" "${_process_log_relative}")

    set(_repeat_json "{}")
    string(JSON _repeat_json SET "${_repeat_json}" repeat ${_repeat})
    _synth_json_set_string(_repeat_json status "pass")
    _synth_json_set_string(_repeat_json report_path "${_validator_report_relative}")
    _synth_json_set_string(_repeat_json report_sha256 "${_validator_report_sha256}")
    _synth_json_set_string(_repeat_json process_log_path "${_process_log_relative}")
    _synth_json_set_string(_repeat_json process_log_sha256 "${_process_log_sha256}")
    set(_exact_command_json "[]")
    set(_command_index 0)
    foreach(_command_token IN LISTS _recorded_command)
        _synth_json_quote(_command_token_json "${_command_token}")
        string(JSON _exact_command_json SET "${_exact_command_json}"
               ${_command_index} "${_command_token_json}")
        math(EXPR _command_index "${_command_index} + 1")
    endforeach()
    string(JSON _repeat_json SET "${_repeat_json}" command "${_exact_command_json}")
    math(EXPR _repeat_index "${_repeat} - 1")
    string(JSON _repeats_json SET "${_repeats_json}" ${_repeat_index} "${_repeat_json}")
endforeach()

set(_unset_environment_json "[]")
set(_unset_environment_index 0)
foreach(_environment_variable IN LISTS _pluginval_environment_variables)
    _synth_json_quote(_environment_variable_json "${_environment_variable}")
    string(JSON _unset_environment_json SET "${_unset_environment_json}"
           ${_unset_environment_index} "${_environment_variable_json}")
    math(EXPR _unset_environment_index "${_unset_environment_index} + 1")
endforeach()

if(DEFINED SYNTH_VST3_VALIDATOR_EXECUTABLE
   AND NOT "${SYNTH_VST3_VALIDATOR_EXECUTABLE}" STREQUAL "")
    set(_vst3_validator_status "pass")
    set(_vst3_validator_reason
        "Configured validator completed within every successful pluginval process")
endif()

set(_options_json "[]")
set(_option_index 0)
foreach(_option IN LISTS _command_options)
    _synth_json_quote(_option_json "${_option}")
    string(JSON _options_json SET "${_options_json}" ${_option_index} "${_option_json}")
    math(EXPR _option_index "${_option_index} + 1")
endforeach()

set(_vst3_validator_json "{}")
_synth_json_set_string(_vst3_validator_json status "${_vst3_validator_status}")
_synth_json_set_string(_vst3_validator_json reason "${_vst3_validator_reason}")
_synth_json_set_string(_vst3_validator_json executable_sha256 "${_vst3_validator_sha256}")
set(_report "{}")
string(JSON _report SET "${_report}" schema_version 1)
_synth_json_set_string(_report status "pass")
_synth_json_set_string(_report tool "pluginval")
_synth_json_set_string(_report tool_version "1.0.4")
_synth_json_set_string(_report executable_sha256 "${_actual_executable_sha256}")
string(JSON _report SET "${_report}" repeat_count ${SYNTH_REPEAT_COUNT})
_synth_json_set_string(_report seed "${SYNTH_SEED}")
string(JSON _report SET "${_report}" strictness_level 10)
string(JSON _report SET "${_report}" timeout_ms 60000)
string(JSON _report SET "${_report}" isolated_process true)
string(JSON _report SET "${_report}" gui_tests true)
string(JSON _report SET "${_report}" environment_sanitized true)
string(JSON _report SET "${_report}" unset_environment_variables
       "${_unset_environment_json}")
_synth_json_set_string(_report plugin_relative_path "${_vst3_relative_path}")
_synth_json_set_string(_report plugin_aggregate_sha256 "${_vst3_aggregate_sha256}")
_synth_json_set_string(_report os "${SYNTH_SYSTEM_NAME}")
_synth_json_set_string(_report architecture "${SYNTH_ARCHITECTURE}")
_synth_json_set_string(_report configuration "${SYNTH_CONFIGURATION}")
string(JSON _report SET "${_report}" command_options "${_options_json}")
string(JSON _report SET "${_report}" vst3_sdk_validator "${_vst3_validator_json}")
string(JSON _report SET "${_report}" repeats "${_repeats_json}")
get_filename_component(_report_directory "${_pluginval_report_path}" DIRECTORY)
file(MAKE_DIRECTORY "${_report_directory}")
file(WRITE "${_pluginval_report_path}" "${_report}\n")
message(STATUS "pluginval 1.0.4 passed ${SYNTH_REPEAT_COUNT} separate isolated processes")
