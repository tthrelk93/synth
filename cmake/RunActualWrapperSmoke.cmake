cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_SMOKE_EXECUTABLE
        SYNTH_BUILD_ROOT
        SYNTH_STAGE_DIRECTORY
        SYNTH_REPEAT_COUNT
        SYNTH_SEED
        SYNTH_REPORT_PATH
        SYNTH_LOG_PATH
        SYNTH_CONFIGURATION
        SYNTH_SYSTEM_NAME
        SYNTH_ARCHITECTURE)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required to run the actual-wrapper smoke")
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

set(_manifest_path "${SYNTH_STAGE_DIRECTORY}/build-manifest.json")
if(NOT EXISTS "${_manifest_path}")
    message(FATAL_ERROR "Staged build manifest is missing: ${_manifest_path}")
endif()

file(READ "${_manifest_path}" _manifest)
string(JSON _product_count ERROR_VARIABLE _product_error
       LENGTH "${_manifest}" products)
if(_product_error)
    message(FATAL_ERROR "Could not parse staged build manifest products: ${_product_error}")
endif()

set(_vst3_count 0)
if(_product_count GREATER 0)
    math(EXPR _last_product "${_product_count} - 1")
    foreach(_product_index RANGE 0 ${_last_product})
        string(JSON _format GET "${_manifest}" products ${_product_index} format)
        if(_format STREQUAL "VST3")
            math(EXPR _vst3_count "${_vst3_count} + 1")
            string(JSON _vst3_relative_path
                   GET "${_manifest}" products ${_product_index} relative_path)
        endif()
    endforeach()
endif()

if(NOT _vst3_count EQUAL 1)
    message(FATAL_ERROR
        "Staged build manifest must declare exactly one VST3 product; found ${_vst3_count}")
endif()
if(_vst3_relative_path MATCHES "\\\\"
   OR IS_ABSOLUTE "${_vst3_relative_path}"
   OR _vst3_relative_path MATCHES "(^|/)[.][.](/|$)")
    message(FATAL_ERROR
        "Staged VST3 relative path is unsafe: ${_vst3_relative_path}")
endif()

set(_staged_vst3 "${SYNTH_STAGE_DIRECTORY}/${_vst3_relative_path}")
if(NOT EXISTS "${_staged_vst3}")
    message(FATAL_ERROR "Staged VST3 product is missing: ${_staged_vst3}")
endif()
file(REAL_PATH "${SYNTH_STAGE_DIRECTORY}" _stage_root)
file(REAL_PATH "${SYNTH_BUILD_ROOT}" _build_root)
cmake_path(ABSOLUTE_PATH SYNTH_BUILD_ROOT NORMALIZE OUTPUT_VARIABLE _build_root_lexical)
file(TO_NATIVE_PATH "${_build_root}" _build_root_native)
file(TO_NATIVE_PATH "${_build_root_lexical}" _build_root_lexical_native)
file(REAL_PATH "${_staged_vst3}" _staged_vst3_real)
cmake_path(IS_PREFIX _stage_root "${_staged_vst3_real}" NORMALIZE _vst3_is_staged)
if(NOT _vst3_is_staged OR _stage_root STREQUAL _staged_vst3_real)
    message(FATAL_ERROR
        "Resolved VST3 product escapes the staged artifact root: ${_staged_vst3_real}")
endif()
file(RELATIVE_PATH _staged_vst3_from_build "${_build_root_lexical}" "${_staged_vst3}")
file(RELATIVE_PATH _report_from_build "${_build_root_lexical}" "${SYNTH_REPORT_PATH}")
file(RELATIVE_PATH _log_from_build "${_build_root_lexical}" "${SYNTH_LOG_PATH}")
foreach(_relative_output IN ITEMS "${_staged_vst3_from_build}" "${_report_from_build}" "${_log_from_build}")
    string(REPLACE "\\" "/" _relative_output "${_relative_output}")
    if(IS_ABSOLUTE "${_relative_output}" OR _relative_output MATCHES "(^|/)[.][.](/|$)")
        message(FATAL_ERROR "Actual-wrapper path must remain beneath SYNTH_BUILD_ROOT: ${_relative_output}")
    endif()
endforeach()
set(_smoke_prefix "")
set(_recorded_prefix "")
set(_virtual_display_provider "native")
if(SYNTH_SYSTEM_NAME STREQUAL "Linux")
    if("$ENV{SYNTH_REUSE_VERIFIED_DISPLAY}" STREQUAL "1")
        if(NOT "$ENV{DISPLAY}" MATCHES "^:[0-9]+([.][0-9]+)?$")
            message(FATAL_ERROR
                "verified Linux display reuse requires a safe local DISPLAY value")
        endif()
        set(_inherited_display "$ENV{DISPLAY}")
        set(_smoke_prefix
            "${CMAKE_COMMAND}" -E env "DISPLAY=${_inherited_display}" --)
        set(_recorded_prefix
            cmake -E env "DISPLAY=<INHERITED_X11_DISPLAY>" --)
        set(_virtual_display_provider "inherited-x11")
    else()
        find_program(_xvfb_run NAMES xvfb-run)
        if(NOT _xvfb_run)
            message(FATAL_ERROR "Linux actual-wrapper GUI smoke requires xvfb-run")
        endif()
        set(_smoke_prefix "${_xvfb_run}" -a)
        set(_recorded_prefix xvfb-run -a)
        set(_virtual_display_provider "xvfb-run")
    endif()
endif()
set(_recorded_command
    ${_recorded_prefix}
    ModelDActualWrapperSmoke
    --plugin "${_staged_vst3_from_build}"
    --repeat "${SYNTH_REPEAT_COUNT}"
    --seed "${SYNTH_SEED}"
    --report "${_report_from_build}"
    --log "${_log_from_build}"
    --config "${SYNTH_CONFIGURATION}"
    --os "${SYNTH_SYSTEM_NAME}"
    --arch "${SYNTH_ARCHITECTURE}")
execute_process(
    COMMAND ${_smoke_prefix} "${SYNTH_SMOKE_EXECUTABLE}"
        --plugin "${_staged_vst3_from_build}"
        --repeat "${SYNTH_REPEAT_COUNT}"
        --seed "${SYNTH_SEED}"
        --report "${_report_from_build}"
        --log "${_log_from_build}"
        --config "${SYNTH_CONFIGURATION}"
        --os "${SYNTH_SYSTEM_NAME}"
        --arch "${SYNTH_ARCHITECTURE}"
    RESULT_VARIABLE _smoke_status
    OUTPUT_VARIABLE _smoke_output
    ERROR_VARIABLE _smoke_error
    WORKING_DIRECTORY "${_build_root_lexical}"
    TIMEOUT 300)

if(NOT _smoke_output STREQUAL "")
    message(STATUS "${_smoke_output}")
endif()
if(NOT _smoke_error STREQUAL "" AND EXISTS "${SYNTH_LOG_PATH}")
    file(APPEND "${SYNTH_LOG_PATH}"
        "captured_stderr_begin\n${_smoke_error}captured_stderr_end\n")
endif()
if(NOT "${_smoke_status}" MATCHES "^[0-9]+$"
   OR NOT "${_smoke_status}" STREQUAL "0")
    message(FATAL_ERROR
        "Actual-wrapper smoke failed with exit ${_smoke_status}:\n${_smoke_error}")
endif()
if(NOT EXISTS "${SYNTH_REPORT_PATH}")
    message(FATAL_ERROR "Actual-wrapper smoke did not create its report")
endif()
file(READ "${SYNTH_REPORT_PATH}" _wrapper_report)
string(JSON _wrapper_report_type ERROR_VARIABLE _wrapper_report_error
       TYPE "${_wrapper_report}")
if(_wrapper_report_error OR NOT _wrapper_report_type STREQUAL "OBJECT")
    message(FATAL_ERROR
        "Actual-wrapper smoke report is invalid JSON: ${_wrapper_report_error}")
endif()
_synth_json_quote(_provider_json "${_virtual_display_provider}")
string(JSON _wrapper_report SET "${_wrapper_report}"
       build virtual_display_provider "${_provider_json}")
set(_command_json "[]")
set(_command_index 0)
foreach(_command_token IN LISTS _recorded_command)
    _synth_json_quote(_command_token_json "${_command_token}")
    string(JSON _command_json SET "${_command_json}"
           ${_command_index} "${_command_token_json}")
    math(EXPR _command_index "${_command_index} + 1")
endforeach()
string(JSON _wrapper_report SET "${_wrapper_report}" command "${_command_json}")
file(WRITE "${SYNTH_REPORT_PATH}" "${_wrapper_report}\n")
foreach(_evidence_file IN ITEMS "${SYNTH_REPORT_PATH}" "${SYNTH_LOG_PATH}")
    if(EXISTS "${_evidence_file}")
        file(READ "${_evidence_file}" _evidence_contents)
        string(REPLACE "${_build_root_native}" "<BUILD_ROOT>" _evidence_contents "${_evidence_contents}")
        string(REPLACE "${_build_root_lexical_native}" "<BUILD_ROOT>" _evidence_contents "${_evidence_contents}")
        string(REPLACE "${_build_root}" "<BUILD_ROOT>" _evidence_contents "${_evidence_contents}")
        string(REPLACE "${_build_root_lexical}" "<BUILD_ROOT>" _evidence_contents "${_evidence_contents}")
        string(REGEX REPLACE "address=0x[0-9A-Fa-f]+"
               "address=<INSTANCE_ADDRESS>" _evidence_contents "${_evidence_contents}")
        file(WRITE "${_evidence_file}" "${_evidence_contents}")
    endif()
endforeach()
