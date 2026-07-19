cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_BUILD_ROOT
        SYNTH_PLUGINVAL_OUTPUT_DIRECTORY
        SYNTH_PLUGINVAL_METADATA_PATH
        SYNTH_PLUGINVAL_STAMP_PATH
        SYNTH_PLUGINVAL_PLATFORM_ASSET
        SYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256
        SYNTH_PLUGINVAL_DOWNLOAD_URL
        SYNTH_PLUGINVAL_EXECUTABLE_RELATIVE_PATH)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required to provision pluginval")
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
cmake_path(ABSOLUTE_PATH SYNTH_PLUGINVAL_OUTPUT_DIRECTORY NORMALIZE
           OUTPUT_VARIABLE _output_directory)
set(_expected_output_directory
    "${_build_root_lexical}/validation-tools/pluginval/v1.0.4")
cmake_path(NORMAL_PATH _expected_output_directory
           OUTPUT_VARIABLE _expected_output_directory)
if(NOT _output_directory STREQUAL _expected_output_directory)
    message(FATAL_ERROR
        "pluginval output must be the fixed build-owned directory: ${_expected_output_directory}")
endif()
cmake_path(ABSOLUTE_PATH SYNTH_PLUGINVAL_METADATA_PATH NORMALIZE
           OUTPUT_VARIABLE _metadata_path)
cmake_path(ABSOLUTE_PATH SYNTH_PLUGINVAL_STAMP_PATH NORMALIZE
           OUTPUT_VARIABLE _stamp_path)
foreach(_owned_path IN ITEMS "${_metadata_path}" "${_stamp_path}")
    cmake_path(IS_PREFIX _build_root_lexical "${_owned_path}" NORMALIZE _is_build_owned)
    if(NOT _is_build_owned OR _owned_path STREQUAL _build_root_lexical)
        message(FATAL_ERROR "pluginval output path is not build-owned: ${_owned_path}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${_output_directory}")
file(REAL_PATH "${_output_directory}" _output_directory_real)
cmake_path(IS_PREFIX _build_root "${_output_directory_real}" NORMALIZE _real_output_is_build_owned)
if(NOT _real_output_is_build_owned)
    message(FATAL_ERROR "pluginval output resolves outside the build root")
endif()
set(_archive_status "not-run")
set(_archive_sha256 "")
set(_archive_source "not-run-executable-override")
set(_executable_source "explicit-executable-override")
if(DEFINED SYNTH_PLUGINVAL_EXECUTABLE_OVERRIDE
   AND NOT "${SYNTH_PLUGINVAL_EXECUTABLE_OVERRIDE}" STREQUAL "")
    if(NOT EXISTS "${SYNTH_PLUGINVAL_EXECUTABLE_OVERRIDE}")
        message(FATAL_ERROR
            "SYNTH_PLUGINVAL_EXECUTABLE does not exist: ${SYNTH_PLUGINVAL_EXECUTABLE_OVERRIDE}")
    endif()
    set(_pluginval_executable "${SYNTH_PLUGINVAL_EXECUTABLE_OVERRIDE}")
else()
    set(_archive_path "${_output_directory}/${SYNTH_PLUGINVAL_PLATFORM_ASSET}")
    set(_archive_source "official-download")
    if(DEFINED SYNTH_PLUGINVAL_ARCHIVE_OVERRIDE
       AND NOT "${SYNTH_PLUGINVAL_ARCHIVE_OVERRIDE}" STREQUAL "")
        if(NOT EXISTS "${SYNTH_PLUGINVAL_ARCHIVE_OVERRIDE}")
            message(FATAL_ERROR
                "SYNTH_PLUGINVAL_ARCHIVE_OVERRIDE does not exist: ${SYNTH_PLUGINVAL_ARCHIVE_OVERRIDE}")
        endif()
        set(_archive_path "${SYNTH_PLUGINVAL_ARCHIVE_OVERRIDE}")
        set(_archive_source "explicit-archive-override")
    else()
        file(DOWNLOAD
            "${SYNTH_PLUGINVAL_DOWNLOAD_URL}"
            "${_archive_path}"
            EXPECTED_HASH "SHA256=${SYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256}"
            TLS_VERIFY ON
            INACTIVITY_TIMEOUT 60
            TIMEOUT 300
            STATUS _download_status
            LOG _download_log)
        list(GET _download_status 0 _download_code)
        if(NOT _download_code EQUAL 0)
            message(FATAL_ERROR
                "pluginval download failed: ${_download_status}\n${_download_log}")
        endif()
    endif()
    file(SHA256 "${_archive_path}" _archive_sha256)
    if(NOT _archive_sha256 STREQUAL SYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256)
        message(FATAL_ERROR
            "pluginval archive SHA-256 mismatch: expected ${SYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256}, got ${_archive_sha256}")
    endif()
    set(_archive_status "pass")
    set(_extract_directory "${_output_directory}/extracted")
    file(REMOVE_RECURSE "${_extract_directory}")
    file(MAKE_DIRECTORY "${_extract_directory}")
    file(ARCHIVE_EXTRACT INPUT "${_archive_path}" DESTINATION "${_extract_directory}")
    set(_pluginval_executable
        "${_extract_directory}/${SYNTH_PLUGINVAL_EXECUTABLE_RELATIVE_PATH}")
    set(_executable_source "pinned-official-archive")
endif()

if(NOT EXISTS "${_pluginval_executable}")
    message(FATAL_ERROR "Provisioned pluginval executable is missing: ${_pluginval_executable}")
endif()
execute_process(
    COMMAND "${_pluginval_executable}" --version
    RESULT_VARIABLE _version_status
    OUTPUT_VARIABLE _version_output
    ERROR_VARIABLE _version_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
    TIMEOUT 15)
if(NOT "${_version_status}" MATCHES "^[0-9]+$"
   OR NOT "${_version_status}" STREQUAL "0")
    message(FATAL_ERROR
        "pluginval --version failed with exit ${_version_status}: ${_version_error}")
endif()
if(NOT _version_output STREQUAL "pluginval - 1.0.4")
    message(FATAL_ERROR
        "Unsupported pluginval version; expected 'pluginval - 1.0.4', got '${_version_output}'")
endif()
file(SHA256 "${_pluginval_executable}" _executable_sha256)

set(_metadata "{}")
string(JSON _metadata SET "${_metadata}" schema_version 1)
_synth_json_set_string(_metadata name "pluginval")
_synth_json_set_string(_metadata version "1.0.4")
_synth_json_set_string(_metadata version_output "${_version_output}")
_synth_json_set_string(_metadata platform_asset "${SYNTH_PLUGINVAL_PLATFORM_ASSET}")
_synth_json_set_string(_metadata archive_source "${_archive_source}")
_synth_json_set_string(_metadata archive_status "${_archive_status}")
_synth_json_set_string(_metadata archive_sha256 "${_archive_sha256}")
_synth_json_set_string(_metadata expected_archive_sha256
                       "${SYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256}")
_synth_json_set_string(_metadata executable_source "${_executable_source}")
_synth_json_set_string(_metadata executable_sha256 "${_executable_sha256}")
cmake_path(ABSOLUTE_PATH _pluginval_executable NORMALIZE
           OUTPUT_VARIABLE _pluginval_executable_absolute)
cmake_path(IS_PREFIX _build_root_lexical "${_pluginval_executable_absolute}"
           NORMALIZE _executable_is_build_owned)
if(_executable_is_build_owned)
    file(RELATIVE_PATH _executable_relative_path
         "${_build_root_lexical}" "${_pluginval_executable_absolute}")
else()
    set(_executable_relative_path "external-override")
endif()
string(REPLACE "\\" "/" _executable_relative_path "${_executable_relative_path}")
_synth_json_set_string(_metadata executable "${_executable_relative_path}")

get_filename_component(_metadata_directory "${_metadata_path}" DIRECTORY)
file(MAKE_DIRECTORY "${_metadata_directory}")
file(WRITE "${_metadata_path}" "${_metadata}\n")
file(WRITE "${_stamp_path}"
     "pluginval - 1.0.4\n${_executable_sha256}\n")
message(STATUS "Provisioned and verified pluginval - 1.0.4")
