cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_BUILD_ROOT
        SYNTH_PLUGINVAL_OUTPUT_DIRECTORY
        SYNTH_PLUGINVAL_METADATA_PATH
        SYNTH_PLUGINVAL_STAMP_PATH
        SYNTH_PLUGINVAL_PLATFORM_ASSET
        SYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256
        SYNTH_PLUGINVAL_DOWNLOAD_URL
        SYNTH_PLUGINVAL_EXECUTABLE_RELATIVE_PATH
        SYNTH_SYSTEM_NAME)
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

if(SYNTH_SYSTEM_NAME STREQUAL "Darwin")
    set(_official_asset "pluginval_macOS.zip")
    set(_official_archive_sha256
        "3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f")
    set(_official_executable_relative_path
        "pluginval.app/Contents/MacOS/pluginval")
elseif(SYNTH_SYSTEM_NAME STREQUAL "Windows")
    set(_official_asset "pluginval_Windows.zip")
    set(_official_archive_sha256
        "c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab")
    set(_official_executable_relative_path "pluginval.exe")
elseif(SYNTH_SYSTEM_NAME STREQUAL "Linux")
    set(_official_asset "pluginval_Linux.zip")
    set(_official_archive_sha256
        "c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352")
    set(_official_executable_relative_path "pluginval")
else()
    message(FATAL_ERROR
        "pluginval v1.0.4 provisioning is unsupported on ${SYNTH_SYSTEM_NAME}")
endif()
set(_official_download_url
    "https://github.com/Tracktion/pluginval/releases/download/v1.0.4/${_official_asset}")
if(NOT SYNTH_PLUGINVAL_PLATFORM_ASSET STREQUAL _official_asset
   OR NOT SYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256 STREQUAL _official_archive_sha256
   OR NOT SYNTH_PLUGINVAL_DOWNLOAD_URL STREQUAL _official_download_url
   OR NOT SYNTH_PLUGINVAL_EXECUTABLE_RELATIVE_PATH STREQUAL _official_executable_relative_path)
    message(FATAL_ERROR
        "pluginval provisioning inputs do not match the hardcoded v1.0.4 pin for ${SYNTH_SYSTEM_NAME}")
endif()

function(_synth_json_set_string json_variable key value)
    _synth_json_quote(_quoted "${value}")
    string(JSON _updated SET "${${json_variable}}" "${key}" "${_quoted}")
    set(${json_variable} "${_updated}" PARENT_SCOPE)
endfunction()

function(_synth_validate_build_owned_directory_ancestry
         build_root_real build_root_lexical candidate_directory description)
    cmake_path(ABSOLUTE_PATH candidate_directory NORMALIZE
               OUTPUT_VARIABLE _candidate_directory)
    cmake_path(IS_PREFIX build_root_lexical "${_candidate_directory}" NORMALIZE
               _candidate_is_lexically_owned)
    if(NOT _candidate_is_lexically_owned
       OR _candidate_directory STREQUAL build_root_lexical)
        message(FATAL_ERROR
            "${description} is not a nested build-owned directory: ${_candidate_directory}")
    endif()

    file(RELATIVE_PATH _relative_directory
         "${build_root_lexical}" "${_candidate_directory}")
    string(REPLACE "\\" "/" _relative_directory "${_relative_directory}")
    if(IS_ABSOLUTE "${_relative_directory}"
       OR _relative_directory MATCHES "(^|/)[.][.](/|$)")
        message(FATAL_ERROR
            "${description} escapes the lexical build root: ${_candidate_directory}")
    endif()

    string(REPLACE "/" ";" _directory_components "${_relative_directory}")
    set(_ancestor "${build_root_lexical}")
    foreach(_directory_component IN LISTS _directory_components)
        set(_ancestor "${_ancestor}/${_directory_component}")
        if(IS_SYMLINK "${_ancestor}")
            message(FATAL_ERROR
                "${description} has a symlink ancestor: ${_ancestor}")
        endif()
        if(EXISTS "${_ancestor}")
            if(NOT IS_DIRECTORY "${_ancestor}")
                message(FATAL_ERROR
                    "${description} has a non-directory ancestor: ${_ancestor}")
            endif()
            file(REAL_PATH "${_ancestor}" _ancestor_real)
            cmake_path(IS_PREFIX build_root_real "${_ancestor_real}" NORMALIZE
                       _ancestor_is_really_owned)
            if(NOT _ancestor_is_really_owned
               OR _ancestor_real STREQUAL build_root_real)
                message(FATAL_ERROR
                    "${description} resolves outside the build root: ${_ancestor}")
            endif()
        endif()
    endforeach()
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
if(NOT _metadata_path STREQUAL "${_build_root_lexical}/validation/pluginval-provision.json"
   OR NOT _stamp_path STREQUAL "${_output_directory}/provision.stamp")
    message(FATAL_ERROR "pluginval metadata/stamp paths must use the fixed build-owned filenames")
endif()

get_filename_component(_metadata_directory "${_metadata_path}" DIRECTORY)
_synth_validate_build_owned_directory_ancestry(
    "${_build_root}" "${_build_root_lexical}" "${_output_directory}"
    "pluginval tool root")
_synth_validate_build_owned_directory_ancestry(
    "${_build_root}" "${_build_root_lexical}" "${_metadata_directory}"
    "pluginval metadata directory")
if(IS_SYMLINK "${_metadata_path}")
    message(FATAL_ERROR "refusing to write symlinked pluginval metadata")
endif()

set(_tool_root_marker "${_output_directory}/.synth-pluginval-tool-root")
if(EXISTS "${_output_directory}" OR IS_SYMLINK "${_output_directory}")
    if(IS_SYMLINK "${_output_directory}")
        message(FATAL_ERROR "refusing to clean a symlinked pluginval tool root")
    endif()
    file(REAL_PATH "${_output_directory}" _existing_output_real)
    cmake_path(IS_PREFIX _build_root "${_existing_output_real}" NORMALIZE
               _existing_output_is_build_owned)
    if(NOT _existing_output_is_build_owned OR _existing_output_real STREQUAL _build_root)
        message(FATAL_ERROR "refusing to clean pluginval tools outside the build root")
    endif()
    if(IS_SYMLINK "${_tool_root_marker}")
        message(FATAL_ERROR "pluginval tool-root ownership marker is symlinked")
    endif()
    if(NOT EXISTS "${_tool_root_marker}")
        # Visual Studio may create the parent of a declared custom-command
        # OUTPUT before launching this script. Adopt that fixed, build-owned
        # directory only while it is empty; never clean unmarked contents.
        file(GLOB _unmarked_tool_entries LIST_DIRECTORIES true
             "${_output_directory}/*"
             "${_output_directory}/.[!.]*"
             "${_output_directory}/..?*")
        if(_unmarked_tool_entries)
            message(FATAL_ERROR
                "unmarked pluginval tool root is not empty: ${_output_directory}")
        endif()
    else()
        file(READ "${_tool_root_marker}" _tool_root_marker_contents)
        if(NOT _tool_root_marker_contents STREQUAL "model-d-pluginval-tool-root-v1\n")
            message(FATAL_ERROR "pluginval tool-root ownership marker is invalid")
        endif()
    endif()
    file(REMOVE_RECURSE "${_output_directory}")
endif()
file(MAKE_DIRECTORY "${_output_directory}")
file(WRITE "${_tool_root_marker}" "model-d-pluginval-tool-root-v1\n")
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
    COMMAND ${_sanitized_environment_command} "${_pluginval_executable}" --version
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

set(_unset_environment_json "[]")
set(_unset_environment_index 0)
foreach(_environment_variable IN LISTS _pluginval_environment_variables)
    _synth_json_quote(_environment_variable_json "${_environment_variable}")
    string(JSON _unset_environment_json SET "${_unset_environment_json}"
           ${_unset_environment_index} "${_environment_variable_json}")
    math(EXPR _unset_environment_index "${_unset_environment_index} + 1")
endforeach()

set(_metadata "{}")
string(JSON _metadata SET "${_metadata}" schema_version 1)
_synth_json_set_string(_metadata name "pluginval")
_synth_json_set_string(_metadata version "1.0.4")
_synth_json_set_string(_metadata version_output "${_version_output}")
_synth_json_set_string(_metadata os "${SYNTH_SYSTEM_NAME}")
_synth_json_set_string(_metadata platform_asset "${SYNTH_PLUGINVAL_PLATFORM_ASSET}")
_synth_json_set_string(_metadata download_url "${SYNTH_PLUGINVAL_DOWNLOAD_URL}")
_synth_json_set_string(_metadata executable_relative_path
                       "${SYNTH_PLUGINVAL_EXECUTABLE_RELATIVE_PATH}")
_synth_json_set_string(_metadata archive_source "${_archive_source}")
_synth_json_set_string(_metadata archive_status "${_archive_status}")
_synth_json_set_string(_metadata archive_sha256 "${_archive_sha256}")
_synth_json_set_string(_metadata expected_archive_sha256
                       "${SYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256}")
_synth_json_set_string(_metadata executable_source "${_executable_source}")
_synth_json_set_string(_metadata executable_sha256 "${_executable_sha256}")
string(JSON _metadata SET "${_metadata}" environment_sanitized true)
string(JSON _metadata SET "${_metadata}" unset_environment_variables
       "${_unset_environment_json}")
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

file(MAKE_DIRECTORY "${_metadata_directory}")
file(WRITE "${_metadata_path}" "${_metadata}\n")
file(WRITE "${_stamp_path}"
     "pluginval - 1.0.4\n${_executable_sha256}\n")
message(STATUS "Provisioned and verified pluginval - 1.0.4")
