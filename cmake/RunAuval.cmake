cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_BUILD_ROOT
        SYNTH_AUVAL_REPORT_PATH
        SYNTH_AUVAL_LOG_PATH
        SYNTH_SYSTEM_NAME
        SYNTH_ARCHITECTURE
        SYNTH_CONFIGURATION
        SYNTH_PRODUCT_CODE
        SYNTH_MANUFACTURER_CODE)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required for auval orchestration")
    endif()
endforeach()

function(_synth_json_quote output_variable input_value)
    set(_escaped "${input_value}")
    string(REPLACE "\\" "\\\\" _escaped "${_escaped}")
    string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
    string(REPLACE "\n" "\\n" _escaped "${_escaped}")
    string(REPLACE "\r" "\\r" _escaped "${_escaped}")
    set(${output_variable} "\"${_escaped}\"" PARENT_SCOPE)
endfunction()

function(_synth_json_set_string json_variable key value)
    _synth_json_quote(_quoted "${value}")
    string(JSON _updated SET "${${json_variable}}" "${key}" "${_quoted}")
    set(${json_variable} "${_updated}" PARENT_SCOPE)
endfunction()

file(REAL_PATH "${SYNTH_BUILD_ROOT}" _build_root)
cmake_path(ABSOLUTE_PATH SYNTH_BUILD_ROOT NORMALIZE OUTPUT_VARIABLE _build_root_lexical)
cmake_path(ABSOLUTE_PATH SYNTH_AUVAL_REPORT_PATH NORMALIZE OUTPUT_VARIABLE _report_path)
cmake_path(ABSOLUTE_PATH SYNTH_AUVAL_LOG_PATH NORMALIZE OUTPUT_VARIABLE _log_path)
foreach(_owned_path IN ITEMS "${_report_path}" "${_log_path}")
    cmake_path(IS_PREFIX _build_root_lexical "${_owned_path}" NORMALIZE _is_build_owned)
    if(NOT _is_build_owned OR _owned_path STREQUAL _build_root_lexical)
        message(FATAL_ERROR "auval evidence path is not build-owned: ${_owned_path}")
    endif()
endforeach()
if(NOT _report_path STREQUAL "${_build_root_lexical}/validation/auval-report.json"
   OR NOT _log_path STREQUAL "${_build_root_lexical}/validation/auval.log")
    message(FATAL_ERROR "auval evidence paths must use the fixed validation filenames")
endif()
get_filename_component(_report_directory "${_report_path}" DIRECTORY)
file(MAKE_DIRECTORY "${_report_directory}")

set(_status "not-run")
set(_reason "auval is only available on macOS")
set(_version "")
set(_exit_code "not-run")
set(_stdout "")
set(_stderr "")
set(_command "auval -v aumu ${SYNTH_PRODUCT_CODE} ${SYNTH_MANUFACTURER_CODE}")
set(_fatal_failure false)
set(_executable_path "")
set(_executable_sha256 "")
if(SYNTH_SYSTEM_NAME STREQUAL "Darwin")
    find_program(_auval_executable NAMES auval)
    if(NOT _auval_executable)
        set(_status "not-run")
        set(_reason "auval executable is unavailable on this macOS system")
    else()
        set(_executable_path "${_auval_executable}")
        file(SHA256 "${_auval_executable}" _executable_sha256)
        execute_process(
            COMMAND "${_auval_executable}" -vers
            RESULT_VARIABLE _version_status
            OUTPUT_VARIABLE _version_stdout
            ERROR_VARIABLE _version_stderr
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
            TIMEOUT 15)
        set(_version "${_version_stdout}${_version_stderr}")
        execute_process(
            COMMAND "${_auval_executable}" -v aumu
                    "${SYNTH_PRODUCT_CODE}" "${SYNTH_MANUFACTURER_CODE}"
            RESULT_VARIABLE _exit_code
            OUTPUT_VARIABLE _stdout
            ERROR_VARIABLE _stderr
            TIMEOUT 300)
        set(_combined_output "${_stdout}\n${_stderr}")
        if("${_exit_code}" MATCHES "^[0-9]+$" AND "${_exit_code}" STREQUAL "0")
            set(_status "pass")
            set(_reason "The registered AU passed the exact auval command")
        elseif("${_exit_code}" MATCHES "^[0-9]+$"
               AND _combined_output MATCHES
                   "(^|\n)FATAL ERROR: didn't find the component(\r?\n|$)")
            set(_status "blocked")
            set(_reason "The AU is not registered in this account; the non-mutating helper did not install it")
        else()
            set(_status "fail")
            set(_reason "An available, registered AU failed auval")
            set(_fatal_failure true)
        endif()
        if(NOT "${_version_status}" MATCHES "^[0-9]+$"
           OR NOT "${_version_status}" STREQUAL "0"
           OR _version STREQUAL "")
            set(_status "fail")
            set(_reason "The available auval tool did not provide required version provenance")
            set(_fatal_failure true)
        endif()
    endif()
endif()

file(WRITE "${_log_path}"
    "mutation=none\ncommand=${_command}\nconfiguration=${SYNTH_CONFIGURATION}\nos=${SYNTH_SYSTEM_NAME}\narchitecture=${SYNTH_ARCHITECTURE}\nversion=${_version}\nstdout_begin\n${_stdout}stdout_end\nstderr_begin\n${_stderr}stderr_end\nexit_code=${_exit_code}\nstatus=${_status}\nreason=${_reason}\n")
file(SHA256 "${_log_path}" _log_sha256)
file(RELATIVE_PATH _log_relative_path "${_build_root_lexical}" "${_log_path}")
string(REPLACE "\\" "/" _log_relative_path "${_log_relative_path}")

set(_report "{}")
string(JSON _report SET "${_report}" schema_version 1)
_synth_json_set_string(_report tool "auval")
_synth_json_set_string(_report status "${_status}")
_synth_json_set_string(_report reason "${_reason}")
_synth_json_set_string(_report command "${_command}")
_synth_json_set_string(_report component_type "aumu")
_synth_json_set_string(_report product_code "${SYNTH_PRODUCT_CODE}")
_synth_json_set_string(_report manufacturer_code "${SYNTH_MANUFACTURER_CODE}")
_synth_json_set_string(_report version "${_version}")
_synth_json_set_string(_report executable_path "${_executable_path}")
_synth_json_set_string(_report executable_sha256 "${_executable_sha256}")
_synth_json_set_string(_report exit_code "${_exit_code}")
_synth_json_set_string(_report log_path "${_log_relative_path}")
_synth_json_set_string(_report log_sha256 "${_log_sha256}")
_synth_json_set_string(_report os "${SYNTH_SYSTEM_NAME}")
_synth_json_set_string(_report architecture "${SYNTH_ARCHITECTURE}")
_synth_json_set_string(_report configuration "${SYNTH_CONFIGURATION}")
string(JSON _report SET "${_report}" mutated_user_state false)
file(WRITE "${_report_path}" "${_report}\n")

if(_fatal_failure)
    message(FATAL_ERROR "auval failed for an available registered AU; see ${_log_path}")
endif()
message(STATUS "auval status: ${_status} (${_reason})")
