cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_SMOKE_EXECUTABLE
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
file(REAL_PATH "${_staged_vst3}" _staged_vst3_real)
cmake_path(IS_PREFIX _stage_root "${_staged_vst3_real}" NORMALIZE _vst3_is_staged)
if(NOT _vst3_is_staged OR _stage_root STREQUAL _staged_vst3_real)
    message(FATAL_ERROR
        "Resolved VST3 product escapes the staged artifact root: ${_staged_vst3_real}")
endif()
execute_process(
    COMMAND "${SYNTH_SMOKE_EXECUTABLE}"
        --plugin "${_staged_vst3}"
        --repeat "${SYNTH_REPEAT_COUNT}"
        --seed "${SYNTH_SEED}"
        --report "${SYNTH_REPORT_PATH}"
        --log "${SYNTH_LOG_PATH}"
        --config "${SYNTH_CONFIGURATION}"
        --os "${SYNTH_SYSTEM_NAME}"
        --arch "${SYNTH_ARCHITECTURE}"
    RESULT_VARIABLE _smoke_status
    OUTPUT_VARIABLE _smoke_output
    ERROR_VARIABLE _smoke_error)

if(NOT _smoke_output STREQUAL "")
    message(STATUS "${_smoke_output}")
endif()
if(NOT _smoke_error STREQUAL "" AND EXISTS "${SYNTH_LOG_PATH}")
    file(APPEND "${SYNTH_LOG_PATH}"
        "captured_stderr_begin\n${_smoke_error}captured_stderr_end\n")
endif()
if(_smoke_status)
    message(FATAL_ERROR
        "Actual-wrapper smoke failed with exit ${_smoke_status}:\n${_smoke_error}")
endif()
