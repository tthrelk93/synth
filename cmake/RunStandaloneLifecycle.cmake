cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_BUILD_ROOT
        SYNTH_STAGE_DIRECTORY
        SYNTH_STANDALONE_REPORT_PATH
        SYNTH_VALIDATION_DIRECTORY
        SYNTH_REPEAT_COUNT
        SYNTH_SYSTEM_NAME
        SYNTH_ARCHITECTURE
        SYNTH_CONFIGURATION
        SYNTH_PROJECT_VERSION
        SYNTH_SOURCE_ROOT)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required to run standalone lifecycle validation")
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

function(_synth_json_set_string json_variable key value)
    _synth_json_quote(_quoted "${value}")
    string(JSON _updated SET "${${json_variable}}" "${key}" "${_quoted}")
    set(${json_variable} "${_updated}" PARENT_SCOPE)
endfunction()

function(_synth_normalize_relative output_variable input_path description)
    string(REPLACE "\\" "/" _path "${input_path}")
    file(TO_CMAKE_PATH "${_path}" _path)
    if(_path STREQUAL ""
       OR IS_ABSOLUTE "${_path}"
       OR _path MATCHES "^[A-Za-z]:"
       OR _path MATCHES "(^|/)[.][.](/|$)")
        message(FATAL_ERROR "${description} has an unsafe relative path: ${input_path}")
    endif()
    cmake_path(NORMAL_PATH _path OUTPUT_VARIABLE _normalized)
    set(${output_variable} "${_normalized}" PARENT_SCOPE)
endfunction()

function(_synth_existing_file_within output_variable root relative_path description)
    _synth_normalize_relative(_relative "${relative_path}" "${description}")
    file(REAL_PATH "${root}" _root_real)
    set(_candidate "${_root_real}/${_relative}")
    if(NOT EXISTS "${_candidate}")
        message(FATAL_ERROR "${description} is missing: ${_relative}")
    endif()
    file(REAL_PATH "${_candidate}" _candidate_real)
    file(RELATIVE_PATH _from_root "${_root_real}" "${_candidate_real}")
    string(REPLACE "\\" "/" _from_root "${_from_root}")
    if(_from_root STREQUAL ""
       OR IS_ABSOLUTE "${_from_root}"
       OR _from_root MATCHES "^[A-Za-z]:"
       OR _from_root MATCHES "(^|/)[.][.](/|$)")
        message(FATAL_ERROR "${description} escapes its trusted root: ${relative_path}")
    endif()
    if(IS_DIRECTORY "${_candidate_real}")
        message(FATAL_ERROR "${description} is a directory: ${relative_path}")
    endif()
    set(${output_variable} "${_candidate_real}" PARENT_SCOPE)
endfunction()

function(_synth_hash_if_file output_variable path)
    if(EXISTS "${path}" AND NOT IS_DIRECTORY "${path}")
        file(SHA256 "${path}" _sha256)
    else()
        set(_sha256 "")
    endif()
    set(${output_variable} "${_sha256}" PARENT_SCOPE)
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

cmake_path(ABSOLUTE_PATH SYNTH_BUILD_ROOT NORMALIZE OUTPUT_VARIABLE _build_root_lexical)
file(REAL_PATH "${SYNTH_STAGE_DIRECTORY}" _stage_root)
file(REAL_PATH "${SYNTH_SOURCE_ROOT}" _source_root)
cmake_path(ABSOLUTE_PATH SYNTH_VALIDATION_DIRECTORY NORMALIZE OUTPUT_VARIABLE _validation_root)
cmake_path(ABSOLUTE_PATH SYNTH_STANDALONE_REPORT_PATH NORMALIZE OUTPUT_VARIABLE _aggregate_path)
if(NOT _validation_root STREQUAL "${_build_root_lexical}/validation"
   OR NOT _aggregate_path STREQUAL "${_build_root_lexical}/validation/standalone-lifecycle-report.json")
    message(FATAL_ERROR "standalone lifecycle outputs must use fixed paths beneath the build validation directory")
endif()
file(REAL_PATH "${SYNTH_BUILD_ROOT}" _build_root)
set(_validation_root "${_build_root}/validation")
set(_aggregate_path "${_validation_root}/standalone-lifecycle-report.json")

set(_build_manifest_path "${_stage_root}/build-manifest.json")
if(NOT EXISTS "${_build_manifest_path}")
    message(FATAL_ERROR "staged build manifest is missing: ${_build_manifest_path}")
endif()
file(READ "${_build_manifest_path}" _build_manifest)
string(JSON _manifest_type ERROR_VARIABLE _manifest_error TYPE "${_build_manifest}")
if(_manifest_error OR NOT _manifest_type STREQUAL "OBJECT")
    message(FATAL_ERROR "staged build manifest is invalid JSON: ${_manifest_error}")
endif()

string(JSON _product_count LENGTH "${_build_manifest}" products)
set(_standalone_count 0)
set(_standalone_executable_relative "")
if(_product_count GREATER 0)
    math(EXPR _last_product "${_product_count} - 1")
    foreach(_product_index RANGE 0 ${_last_product})
        string(JSON _format GET "${_build_manifest}" products ${_product_index} format)
        if(NOT _format STREQUAL "Standalone")
            continue()
        endif()
        math(EXPR _standalone_count "${_standalone_count} + 1")
        string(JSON _standalone_product_relative GET "${_build_manifest}" products ${_product_index} relative_path)
        string(JSON _standalone_payload_root GET "${_build_manifest}" products ${_product_index} payload_root_relative_path)
        string(JSON _standalone_product_sha256 GET "${_build_manifest}" products ${_product_index} aggregate_sha256)
        _synth_normalize_relative(_standalone_product_relative "${_standalone_product_relative}" "Standalone product")
        _synth_normalize_relative(_standalone_payload_root "${_standalone_payload_root}" "Standalone payload root")
        set(_aggregate_input "")
        set(_executable_candidates "")
        string(JSON _file_count LENGTH "${_build_manifest}" products ${_product_index} files)
        if(_file_count LESS 1)
            message(FATAL_ERROR "Standalone product has no payload inventory")
        endif()
        math(EXPR _last_file "${_file_count} - 1")
        foreach(_file_index RANGE 0 ${_last_file})
            string(JSON _file_relative GET "${_build_manifest}" products ${_product_index} files ${_file_index} path)
            string(JSON _file_sha256 GET "${_build_manifest}" products ${_product_index} files ${_file_index} sha256)
            _synth_normalize_relative(_file_relative "${_file_relative}" "Standalone payload")
            set(_stage_file_relative "${_standalone_payload_root}/${_file_relative}")
            _synth_existing_file_within(_stage_file "${_stage_root}" "${_stage_file_relative}" "Standalone payload")
            file(SHA256 "${_stage_file}" _actual_file_sha256)
            if(NOT _actual_file_sha256 STREQUAL _file_sha256)
                message(FATAL_ERROR "Standalone payload hash mismatch: ${_stage_file_relative}")
            endif()
            string(APPEND _aggregate_input "${_file_sha256}  ${_file_relative}\n")
            if(SYNTH_SYSTEM_NAME STREQUAL "Darwin")
                if(_file_relative MATCHES "^Contents/MacOS/[^/]+$")
                    list(APPEND _executable_candidates "${_stage_file_relative}|${_file_sha256}")
                endif()
            elseif(_stage_file_relative STREQUAL _standalone_product_relative)
                list(APPEND _executable_candidates "${_stage_file_relative}|${_file_sha256}")
            endif()
        endforeach()
        string(SHA256 _actual_product_sha256 "${_aggregate_input}")
        if(NOT _actual_product_sha256 STREQUAL _standalone_product_sha256)
            message(FATAL_ERROR "Standalone product aggregate hash mismatch")
        endif()
        list(LENGTH _executable_candidates _executable_count)
        if(NOT _executable_count EQUAL 1)
            message(FATAL_ERROR "Standalone product must declare exactly one executable payload")
        endif()
        list(GET _executable_candidates 0 _executable_entry)
        string(REPLACE "|" ";" _executable_parts "${_executable_entry}")
        list(GET _executable_parts 0 _standalone_executable_relative)
        list(GET _executable_parts 1 _standalone_executable_sha256)
    endforeach()
endif()
if(NOT _standalone_count EQUAL 1)
    message(FATAL_ERROR "staged build manifest must declare exactly one Standalone product")
endif()
_synth_existing_file_within(_standalone_executable "${_stage_root}"
                            "${_standalone_executable_relative}" "Standalone executable")

file(RELATIVE_PATH _stage_from_build "${_build_root}" "${_stage_root}")
string(REPLACE "\\" "/" _stage_from_build "${_stage_from_build}")
_synth_normalize_relative(_stage_from_build "${_stage_from_build}" "Stage directory")

set(_standalone_root "${_validation_root}/standalone")
set(_standalone_root_lexical
    "${_build_root_lexical}/validation/standalone")
set(_standalone_root_marker
    "${_build_root}/validation-ownership/standalone-evidence-root.marker")
set(_standalone_marker_directory_lexical
    "${_build_root_lexical}/validation-ownership")
_synth_validate_build_owned_directory_ancestry(
    "${_build_root}" "${_build_root_lexical}"
    "${_standalone_root_lexical}" "standalone evidence root")
_synth_validate_build_owned_directory_ancestry(
    "${_build_root}" "${_build_root_lexical}"
    "${_standalone_marker_directory_lexical}"
    "standalone evidence ownership directory")
if(IS_SYMLINK "${_standalone_root_marker}")
    message(FATAL_ERROR "standalone evidence ownership marker is symlinked")
endif()
if(IS_SYMLINK "${_aggregate_path}")
    message(FATAL_ERROR "standalone lifecycle aggregate report is symlinked")
endif()
if(EXISTS "${_standalone_root}" OR IS_SYMLINK "${_standalone_root}")
    if(IS_SYMLINK "${_standalone_root}")
        message(FATAL_ERROR "refusing to clean a symlinked standalone evidence root")
    endif()
    file(REAL_PATH "${_standalone_root}" _existing_standalone_root_real)
    cmake_path(IS_PREFIX _validation_root "${_existing_standalone_root_real}"
               NORMALIZE _existing_standalone_is_validation_owned)
    if(NOT _existing_standalone_is_validation_owned
       OR _existing_standalone_root_real STREQUAL _validation_root)
        message(FATAL_ERROR "refusing to clean standalone evidence outside validation")
    endif()
    if(NOT EXISTS "${_standalone_root_marker}")
        message(FATAL_ERROR
            "refusing to clean an unmarked standalone evidence root: ${_standalone_root}")
    endif()
    file(READ "${_standalone_root_marker}" _standalone_marker_contents)
    if(NOT _standalone_marker_contents STREQUAL "model-d-standalone-evidence-root-v1\n")
        message(FATAL_ERROR "standalone evidence-root ownership marker is invalid")
    endif()
    file(REMOVE_RECURSE "${_standalone_root}")
endif()
file(MAKE_DIRECTORY "${_standalone_root}")
get_filename_component(_standalone_marker_directory
                       "${_standalone_root_marker}" DIRECTORY)
file(MAKE_DIRECTORY "${_standalone_marker_directory}")
file(WRITE "${_standalone_root_marker}"
     "model-d-standalone-evidence-root-v1\n")

set(_runs "[]")
set(_run_index 0)
set(_all_processes_passed true)
set(_determinism_failure "")
foreach(_mode IN ITEMS normal invalid no-device)
    string(REPLACE "-" "_" _mode_key "${_mode}")
    foreach(_repeat RANGE 1 ${SYNTH_REPEAT_COUNT})
        set(_run_relative "validation/standalone/${_mode}/repeat-${_repeat}")
        set(_run_directory "${_build_root}/${_run_relative}")
        set(_report_relative "${_run_relative}/report.json")
        set(_screenshot_relative "${_run_relative}/screenshot.png")
        set(_log_relative "${_run_relative}/process.log")
        set(_report_path "${_build_root}/${_report_relative}")
        set(_screenshot_path "${_build_root}/${_screenshot_relative}")
        set(_log_path "${_build_root}/${_log_relative}")
        set(_runtime_isolation_root "${_standalone_root}/runtime-isolation")
        set(_isolation_relative "validation/standalone/runtime-isolation/${_mode}")
        set(_isolation_root "${_build_root}/${_isolation_relative}")
        if(EXISTS "${_runtime_isolation_root}" OR IS_SYMLINK "${_runtime_isolation_root}")
            if(IS_SYMLINK "${_runtime_isolation_root}")
                message(FATAL_ERROR "refusing to clean a symlinked standalone runtime-isolation root")
            endif()
            file(REMOVE_RECURSE "${_runtime_isolation_root}")
        endif()
        foreach(_directory IN ITEMS home config data cache appdata localappdata tmp settings presets)
            file(MAKE_DIRECTORY "${_isolation_root}/${_directory}")
        endforeach()
        set(_settings_relative "${_isolation_relative}/settings/MiniMoog.settings")
        set(_preset_relative "${_isolation_relative}/presets")
        set(_settings_path "${_build_root}/${_settings_relative}")
        set(_preset_path "${_build_root}/${_preset_relative}")

        set(_normalized_command cmake -E env
            "HOME=${_isolation_relative}/home"
            "USERPROFILE=${_isolation_relative}/home"
            "XDG_CONFIG_HOME=${_isolation_relative}/config"
            "XDG_DATA_HOME=${_isolation_relative}/data"
            "XDG_CACHE_HOME=${_isolation_relative}/cache"
            "APPDATA=${_isolation_relative}/appdata"
            "LOCALAPPDATA=${_isolation_relative}/localappdata"
            "TMPDIR=${_isolation_relative}/tmp"
            "TEMP=${_isolation_relative}/tmp"
            "TMP=${_isolation_relative}/tmp")
        set(_actual_command "${CMAKE_COMMAND}" -E env
            "HOME=${_isolation_root}/home"
            "USERPROFILE=${_isolation_root}/home"
            "XDG_CONFIG_HOME=${_isolation_root}/config"
            "XDG_DATA_HOME=${_isolation_root}/data"
            "XDG_CACHE_HOME=${_isolation_root}/cache"
            "APPDATA=${_isolation_root}/appdata"
            "LOCALAPPDATA=${_isolation_root}/localappdata"
            "TMPDIR=${_isolation_root}/tmp"
            "TEMP=${_isolation_root}/tmp"
            "TMP=${_isolation_root}/tmp")
        if(SYNTH_SYSTEM_NAME STREQUAL "Linux")
            list(APPEND _normalized_command xvfb-run -a)
            list(APPEND _actual_command xvfb-run -a)
        endif()
        list(APPEND _normalized_command
            "${_stage_from_build}/${_standalone_executable_relative}"
            --synth-lifecycle-test "${_mode}"
            --report "${_report_relative}"
            --screenshot "${_screenshot_relative}"
            --settings "${_settings_relative}"
            --preset-dir "${_preset_relative}")
        list(APPEND _actual_command
            "${_standalone_executable}"
            --synth-lifecycle-test "${_mode}"
            --report "${_report_path}"
            --screenshot "${_screenshot_path}"
            --settings "${_settings_path}"
            --preset-dir "${_preset_path}")

        execute_process(
            COMMAND ${_actual_command}
            WORKING_DIRECTORY "${_build_root}"
            TIMEOUT 180
            RESULT_VARIABLE _process_status
            OUTPUT_VARIABLE _stdout
            ERROR_VARIABLE _stderr
            ENCODING UTF-8)
        string(REPLACE "${_build_root}" "<BUILD_ROOT>" _stdout "${_stdout}")
        string(REPLACE "${_build_root}" "<BUILD_ROOT>" _stderr "${_stderr}")
        string(REPLACE "\\" "/" _stdout "${_stdout}")
        string(REPLACE "\\" "/" _stderr "${_stderr}")
        foreach(_stream IN ITEMS _stdout _stderr)
            string(REGEX REPLACE "Log started:[^\r\n]*" "Log started: <NORMALIZED>"
                   ${_stream} "${${_stream}}")
            string(REGEX REPLACE "0x[0-9A-Fa-f]+" "<POINTER>"
                   ${_stream} "${${_stream}}")
        endforeach()
        set(_log_command ${_normalized_command})
        string(REPLACE "repeat-${_repeat}" "repeat-<REPEAT>" _log_command "${_log_command}")
        list(JOIN _log_command "\ncommand-token=" _command_lines)
        file(WRITE "${_log_path}"
            "command-token=${_command_lines}\nexit-code=${_process_status}\nstdout-begin\n${_stdout}stdout-end\nstderr-begin\n${_stderr}stderr-end\n")

        set(_run_status "pass")
        if(NOT _process_status STREQUAL "0"
           OR NOT EXISTS "${_report_path}"
           OR NOT EXISTS "${_screenshot_path}")
            set(_run_status "fail")
            set(_all_processes_passed false)
        else()
            file(READ "${_report_path}" _application_report)
            string(JSON _application_type ERROR_VARIABLE _application_error TYPE "${_application_report}")
            if(_application_error OR NOT _application_type STREQUAL "OBJECT")
                set(_run_status "fail")
                set(_all_processes_passed false)
            else()
                string(JSON _application_status ERROR_VARIABLE _status_error GET "${_application_report}" status)
                if(_status_error OR NOT _application_status STREQUAL "pass")
                    set(_run_status "fail")
                    set(_all_processes_passed false)
                else()
                    string(JSON _reported_settings ERROR_VARIABLE _settings_error
                           GET "${_application_report}" settings path)
                    string(JSON _settings_match ERROR_VARIABLE _settings_match_error
                           GET "${_application_report}" settings matches_requested_path)
                    string(JSON _reported_presets ERROR_VARIABLE _presets_error
                           GET "${_application_report}" presets path)
                    string(JSON _presets_match ERROR_VARIABLE _presets_match_error
                           GET "${_application_report}" presets matches_requested_path)
                    if(_settings_error OR _settings_match_error OR _presets_error OR _presets_match_error
                       OR NOT _reported_settings STREQUAL _settings_path
                       OR NOT _reported_presets STREQUAL _preset_path
                       OR NOT _settings_match OR NOT _presets_match
                       OR NOT EXISTS "${_settings_path}" OR IS_DIRECTORY "${_settings_path}"
                       OR IS_SYMLINK "${_settings_path}"
                       OR NOT IS_DIRECTORY "${_preset_path}" OR IS_SYMLINK "${_preset_path}")
                        set(_run_status "fail")
                        set(_all_processes_passed false)
                    else()
                        file(REAL_PATH "${_isolation_root}" _isolation_root_real)
                        file(REAL_PATH "${_settings_path}" _settings_path_real)
                        file(REAL_PATH "${_preset_path}" _preset_path_real)
                        cmake_path(IS_PREFIX _isolation_root_real "${_settings_path_real}"
                                   NORMALIZE _settings_is_isolated)
                        cmake_path(IS_PREFIX _isolation_root_real "${_preset_path_real}"
                                   NORMALIZE _presets_are_isolated)
                        if(NOT _settings_is_isolated OR NOT _presets_are_isolated)
                            set(_run_status "fail")
                            set(_all_processes_passed false)
                        else()
                            _synth_json_quote(_settings_relative_json "${_settings_relative}")
                            _synth_json_quote(_preset_relative_json "${_preset_relative}")
                            string(JSON _application_report SET "${_application_report}"
                                   settings path "${_settings_relative_json}")
                            string(JSON _application_report SET "${_application_report}"
                                   presets path "${_preset_relative_json}")
                            file(WRITE "${_report_path}" "${_application_report}\n")
                        endif()
                    endif()
                endif()
            endif()
        endif()

        file(REMOVE_RECURSE "${_runtime_isolation_root}")
        if(EXISTS "${_runtime_isolation_root}" OR IS_SYMLINK "${_runtime_isolation_root}")
            message(FATAL_ERROR "standalone runtime-isolation cleanup failed")
        endif()

        _synth_hash_if_file(_report_sha256 "${_report_path}")
        _synth_hash_if_file(_screenshot_sha256 "${_screenshot_path}")
        file(SHA256 "${_log_path}" _log_sha256)
        set(_process_log_sha256 "${_log_sha256}")

        set(_command_json "[]")
        set(_token_index 0)
        foreach(_token IN LISTS _normalized_command)
            _synth_json_quote(_quoted_token "${_token}")
            string(JSON _command_json SET "${_command_json}" ${_token_index} "${_quoted_token}")
            math(EXPR _token_index "${_token_index} + 1")
        endforeach()
        set(_run "{}")
        _synth_json_set_string(_run mode "${_mode}")
        string(JSON _run SET "${_run}" repeat ${_repeat})
        _synth_json_set_string(_run status "${_run_status}")
        _synth_json_set_string(_run exit_code "${_process_status}")
        string(JSON _run SET "${_run}" command "${_command_json}")
        _synth_json_set_string(_run report_path "${_report_relative}")
        _synth_json_set_string(_run report_sha256 "${_report_sha256}")
        _synth_json_set_string(_run screenshot_path "${_screenshot_relative}")
        _synth_json_set_string(_run screenshot_sha256 "${_screenshot_sha256}")
        _synth_json_set_string(_run process_log_path "${_log_relative}")
        _synth_json_set_string(_run process_log_sha256 "${_log_sha256}")
        string(JSON _runs SET "${_runs}" ${_run_index} "${_run}")

        if(_run_status STREQUAL "pass")
            foreach(_kind IN ITEMS report screenshot process_log)
                set(_hash "${_${_kind}_sha256}")
                set(_baseline_variable "_${_mode_key}_${_kind}_sha256")
                if(_repeat EQUAL 1)
                    set(${_baseline_variable} "${_hash}")
                elseif(NOT _hash STREQUAL "${${_baseline_variable}}")
                    set(_all_processes_passed false)
                    string(APPEND _determinism_failure
                           "${_mode} ${_kind} evidence differs at repeat ${_repeat}; ")
                endif()
            endforeach()
        endif()
        math(EXPR _run_index "${_run_index} + 1")
    endforeach()
endforeach()

set(_executable "{}")
_synth_json_set_string(_executable product_relative_path "${_standalone_product_relative}")
_synth_json_set_string(_executable product_aggregate_sha256 "${_standalone_product_sha256}")
_synth_json_set_string(_executable relative_path "${_standalone_executable_relative}")
_synth_json_set_string(_executable sha256 "${_standalone_executable_sha256}")
set(_environment "{}")
_synth_json_set_string(_environment os "${SYNTH_SYSTEM_NAME}")
_synth_json_set_string(_environment architecture "${SYNTH_ARCHITECTURE}")
_synth_json_set_string(_environment configuration "${SYNTH_CONFIGURATION}")
set(_aggregate "{}")
string(JSON _aggregate SET "${_aggregate}" schema_version 1)
_synth_json_set_string(_aggregate tool_version "${SYNTH_PROJECT_VERSION}")
if(_all_processes_passed)
    _synth_json_set_string(_aggregate status "pass")
else()
    _synth_json_set_string(_aggregate status "fail")
endif()
string(JSON _aggregate SET "${_aggregate}" environment "${_environment}")
string(JSON _aggregate SET "${_aggregate}" repeat_count ${SYNTH_REPEAT_COUNT})
string(JSON _aggregate SET "${_aggregate}" executable "${_executable}")
string(JSON _aggregate SET "${_aggregate}" runs "${_runs}")
file(WRITE "${_aggregate_path}" "${_aggregate}\n")

if(NOT _determinism_failure STREQUAL "")
    message(FATAL_ERROR "standalone lifecycle evidence is nondeterministic: ${_determinism_failure}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSYNTH_BUILD_ROOT=${_build_root}"
        "-DSYNTH_STAGE_DIRECTORY=${_stage_root}"
        "-DSYNTH_STANDALONE_REPORT_PATH=${_aggregate_path}"
        "-DSYNTH_REPEAT_COUNT=${SYNTH_REPEAT_COUNT}"
        "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
        "-DSYNTH_ARCHITECTURE=${SYNTH_ARCHITECTURE}"
        "-DSYNTH_CONFIGURATION=${SYNTH_CONFIGURATION}"
        "-DSYNTH_PROJECT_VERSION=${SYNTH_PROJECT_VERSION}"
        -P "${_source_root}/cmake/VerifyStandaloneLifecycleEvidence.cmake"
    RESULT_VARIABLE _verification_status
    OUTPUT_VARIABLE _verification_stdout
    ERROR_VARIABLE _verification_stderr
    ENCODING UTF-8)
if(NOT _verification_status EQUAL 0)
    message(FATAL_ERROR
        "Standalone lifecycle evidence verification failed:\n${_verification_stdout}${_verification_stderr}")
endif()
message(STATUS "Standalone lifecycle validation passed (${_run_index} fresh processes)")
