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
if(NOT _link_result STREQUAL "0")
    message(FATAL_ERROR "Unable to create standalone symlink negative fixture: ${_link_result}")
endif()
_synth_run_runner_expect_failure("symlinked evidence root" "symlinked standalone evidence root")
file(REMOVE "${_standalone_root}")
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
message(STATUS "Standalone lifecycle positive and negative contracts passed")
