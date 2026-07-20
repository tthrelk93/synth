cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_BUILD_ROOT
        SYNTH_STAGE_DIRECTORY
        SYNTH_VALIDATION_MANIFEST_PATH
        SYNTH_PLUGINVAL_METADATA_PATH
        SYNTH_PLUGINVAL_REPORT_PATH
        SYNTH_WRAPPER_REPORT_PATH
        SYNTH_WRAPPER_LOG_PATH
        SYNTH_AUVAL_REPORT_PATH
        SYNTH_STANDALONE_REPORT_PATH
        SYNTH_HOST_MATRIX_PATH
        SYNTH_REPEAT_COUNT
        SYNTH_SEED
        SYNTH_SYSTEM_NAME
        SYNTH_ARCHITECTURE
        SYNTH_CONFIGURATION
        SYNTH_PROJECT_VERSION
        SYNTH_EXPECTED_EVIDENCE)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required to generate validation metadata")
    endif()
endforeach()
string(REPLACE "\\;" ";" SYNTH_EXPECTED_EVIDENCE "${SYNTH_EXPECTED_EVIDENCE}")

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

function(_synth_read_json output_variable path description)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "${description} is missing: ${path}")
    endif()
    file(READ "${path}" _json)
    string(JSON _type ERROR_VARIABLE _error TYPE "${_json}")
    if(_error OR NOT _type STREQUAL "OBJECT")
        message(FATAL_ERROR "${description} is invalid JSON: ${_error}")
    endif()
    set(${output_variable} "${_json}" PARENT_SCOPE)
endfunction()

function(_synth_require_status status context)
    if(NOT status MATCHES "^(not-run|in-progress|blocked|fail|pass)$")
        message(FATAL_ERROR "${context} has an unsupported status token: ${status}")
    endif()
endfunction()

file(REAL_PATH "${SYNTH_BUILD_ROOT}" _build_root)
cmake_path(ABSOLUTE_PATH SYNTH_BUILD_ROOT NORMALIZE OUTPUT_VARIABLE _build_root_lexical)
file(REAL_PATH "${SYNTH_STAGE_DIRECTORY}" _stage_root)
cmake_path(ABSOLUTE_PATH SYNTH_STAGE_DIRECTORY NORMALIZE OUTPUT_VARIABLE _stage_root_lexical)
cmake_path(ABSOLUTE_PATH SYNTH_VALIDATION_MANIFEST_PATH NORMALIZE
           OUTPUT_VARIABLE _validation_manifest_path)
cmake_path(IS_PREFIX _stage_root_lexical "${_validation_manifest_path}" NORMALIZE _manifest_is_staged)
if(NOT _manifest_is_staged
   OR NOT _validation_manifest_path STREQUAL "${_stage_root_lexical}/validation-manifest.json")
    message(FATAL_ERROR "validation manifest must be the fixed adjacent stage metadata path")
endif()

set(_build_manifest_path "${_stage_root}/build-manifest.json")
_synth_read_json(_build_manifest "${_build_manifest_path}" "build manifest")
file(SHA256 "${_build_manifest_path}" _build_manifest_sha256)
_synth_read_json(_provision "${SYNTH_PLUGINVAL_METADATA_PATH}" "pluginval provision report")
_synth_read_json(_pluginval "${SYNTH_PLUGINVAL_REPORT_PATH}" "pluginval run report")
_synth_read_json(_wrapper "${SYNTH_WRAPPER_REPORT_PATH}" "actual-wrapper report")
_synth_read_json(_auval "${SYNTH_AUVAL_REPORT_PATH}" "auval report")
_synth_read_json(_standalone "${SYNTH_STANDALONE_REPORT_PATH}" "standalone lifecycle report")
_synth_read_json(_host_definition "${SYNTH_HOST_MATRIX_PATH}" "required host matrix")

string(JSON _build_os GET "${_build_manifest}" build os)
string(JSON _build_architecture GET "${_build_manifest}" build architecture)
string(JSON _build_configuration GET "${_build_manifest}" build configuration)
string(JSON _provision_os GET "${_provision}" os)
if(NOT _build_os STREQUAL SYNTH_SYSTEM_NAME
   OR NOT _build_architecture STREQUAL SYNTH_ARCHITECTURE
   OR NOT _build_configuration STREQUAL SYNTH_CONFIGURATION
   OR NOT _provision_os STREQUAL SYNTH_SYSTEM_NAME)
    message(FATAL_ERROR "build/provision environment does not match the configured validation environment")
endif()

string(JSON _product_count LENGTH "${_build_manifest}" products)
set(_vst3_count 0)
set(_standalone_count 0)
if(_product_count GREATER 0)
    math(EXPR _last_product "${_product_count} - 1")
    foreach(_product_index RANGE 0 ${_last_product})
        string(JSON _format GET "${_build_manifest}" products ${_product_index} format)
        if(_format STREQUAL "VST3")
            math(EXPR _vst3_count "${_vst3_count} + 1")
            string(JSON _final_vst3_relative_path GET "${_build_manifest}" products ${_product_index} relative_path)
            string(JSON _final_vst3_sha256 GET "${_build_manifest}" products ${_product_index} aggregate_sha256)
        elseif(_format STREQUAL "Standalone")
            math(EXPR _standalone_count "${_standalone_count} + 1")
            string(JSON _final_standalone_relative_path GET "${_build_manifest}" products ${_product_index} relative_path)
            string(JSON _final_standalone_sha256 GET "${_build_manifest}" products ${_product_index} aggregate_sha256)
        endif()
    endforeach()
endif()
if(NOT _vst3_count EQUAL 1)
    message(FATAL_ERROR "final build manifest must declare exactly one VST3 product")
endif()
if(NOT _standalone_count EQUAL 1)
    message(FATAL_ERROR "final build manifest must declare exactly one Standalone product")
endif()

string(JSON _standalone_status GET "${_standalone}" status)
string(JSON _standalone_version GET "${_standalone}" tool_version)
string(JSON _standalone_repeat_count GET "${_standalone}" repeat_count)
string(JSON _standalone_os GET "${_standalone}" environment os)
string(JSON _standalone_arch GET "${_standalone}" environment architecture)
string(JSON _standalone_configuration GET "${_standalone}" environment configuration)
string(JSON _standalone_virtual_display_provider GET "${_standalone}" environment virtual_display_provider)
if(_build_os STREQUAL "Linux")
    if(NOT _standalone_virtual_display_provider MATCHES "^(inherited-x11|xvfb-run)$")
        message(FATAL_ERROR "standalone report has an invalid Linux virtual-display provider")
    endif()
elseif(NOT _standalone_virtual_display_provider STREQUAL "native")
    message(FATAL_ERROR "standalone report has an invalid native display provider")
endif()
string(JSON _standalone_product_path GET "${_standalone}" executable product_relative_path)
string(JSON _standalone_product_sha256 GET "${_standalone}" executable product_aggregate_sha256)
string(JSON _standalone_executable_path GET "${_standalone}" executable relative_path)
string(JSON _standalone_executable_sha256 GET "${_standalone}" executable sha256)
string(JSON _standalone_run_count LENGTH "${_standalone}" runs)
math(EXPR _expected_standalone_runs "3 * ${SYNTH_REPEAT_COUNT}")
if(NOT _standalone_status STREQUAL "pass"
   OR NOT _standalone_version STREQUAL SYNTH_PROJECT_VERSION
   OR NOT _standalone_repeat_count STREQUAL SYNTH_REPEAT_COUNT
   OR NOT _standalone_os STREQUAL SYNTH_SYSTEM_NAME
   OR NOT _standalone_arch STREQUAL SYNTH_ARCHITECTURE
   OR NOT _standalone_configuration STREQUAL SYNTH_CONFIGURATION
   OR NOT _standalone_configuration STREQUAL _build_configuration
   OR NOT _standalone_product_path STREQUAL _final_standalone_relative_path
   OR NOT _standalone_product_sha256 STREQUAL _final_standalone_sha256
   OR _standalone_executable_path STREQUAL ""
   OR _standalone_executable_sha256 STREQUAL ""
   OR NOT _standalone_run_count EQUAL _expected_standalone_runs)
    message(FATAL_ERROR "standalone lifecycle report does not satisfy the actual-product contract")
endif()

string(JSON _pluginval_status GET "${_pluginval}" status)
string(JSON _pluginval_version GET "${_pluginval}" tool_version)
string(JSON _pluginval_repeat_count GET "${_pluginval}" repeat_count)
string(JSON _pluginval_seed GET "${_pluginval}" seed)
string(JSON _pluginval_strictness GET "${_pluginval}" strictness_level)
string(JSON _pluginval_timeout GET "${_pluginval}" timeout_ms)
string(JSON _pluginval_isolated GET "${_pluginval}" isolated_process)
string(JSON _pluginval_gui GET "${_pluginval}" gui_tests)
string(JSON _pluginval_environment_sanitized GET "${_pluginval}" environment_sanitized)
string(JSON _pluginval_plugin_path GET "${_pluginval}" plugin_relative_path)
string(JSON _pluginval_plugin_sha256 GET "${_pluginval}" plugin_aggregate_sha256)
string(JSON _pluginval_os GET "${_pluginval}" os)
string(JSON _pluginval_architecture GET "${_pluginval}" architecture)
string(JSON _pluginval_configuration GET "${_pluginval}" configuration)
string(JSON _pluginval_virtual_display_provider GET "${_pluginval}" virtual_display_provider)
if(_build_os STREQUAL "Linux")
    if(NOT _pluginval_virtual_display_provider MATCHES "^(inherited-x11|xvfb-run)$")
        message(FATAL_ERROR "pluginval report has an invalid Linux virtual-display provider")
    endif()
elseif(NOT _pluginval_virtual_display_provider STREQUAL "native")
    message(FATAL_ERROR "pluginval report has an invalid native display provider")
endif()
if(NOT _pluginval_status STREQUAL "pass"
   OR NOT _pluginval_version STREQUAL "1.0.4"
   OR NOT _pluginval_repeat_count STREQUAL SYNTH_REPEAT_COUNT
   OR NOT _pluginval_seed STREQUAL SYNTH_SEED
   OR NOT _pluginval_strictness STREQUAL "10"
   OR NOT _pluginval_timeout STREQUAL "60000"
   OR NOT _pluginval_isolated
   OR NOT _pluginval_gui
   OR NOT _pluginval_environment_sanitized
   OR NOT _pluginval_plugin_path STREQUAL _final_vst3_relative_path
   OR NOT _pluginval_plugin_sha256 STREQUAL _final_vst3_sha256
   OR NOT _pluginval_os STREQUAL _build_os
   OR NOT _pluginval_architecture STREQUAL _build_architecture
   OR NOT _pluginval_configuration STREQUAL _build_configuration)
    message(FATAL_ERROR "pluginval report does not satisfy the pinned validation contract")
endif()
string(JSON _wrapper_status GET "${_wrapper}" status)
string(JSON _wrapper_repeat_count GET "${_wrapper}" repeat_count)
string(JSON _wrapper_seed GET "${_wrapper}" seed)
string(JSON _wrapper_plugin_path GET "${_wrapper}" plugin relative_path)
string(JSON _wrapper_plugin_sha256 GET "${_wrapper}" plugin aggregate_sha256)
string(JSON _wrapper_os GET "${_wrapper}" build os)
string(JSON _wrapper_architecture GET "${_wrapper}" build architecture)
string(JSON _wrapper_configuration GET "${_wrapper}" build configuration)
string(JSON _wrapper_virtual_display_provider GET "${_wrapper}" build virtual_display_provider)
if(_build_os STREQUAL "Linux")
    if(NOT _wrapper_virtual_display_provider MATCHES "^(inherited-x11|xvfb-run)$")
        message(FATAL_ERROR "actual-wrapper report has an invalid Linux virtual-display provider")
    endif()
elseif(NOT _wrapper_virtual_display_provider STREQUAL "native")
    message(FATAL_ERROR "actual-wrapper report has an invalid native display provider")
endif()
file(RELATIVE_PATH _stage_relative_from_build "${_build_root}" "${_stage_root}")
string(REPLACE "\\" "/" _stage_relative_from_build "${_stage_relative_from_build}")
set(_expected_wrapper_plugin_path
    "${_stage_relative_from_build}/${_final_vst3_relative_path}")
set(_expected_wrapper_command "")
if(_wrapper_virtual_display_provider STREQUAL "inherited-x11")
    list(APPEND _expected_wrapper_command
         cmake -E env "DISPLAY=<INHERITED_X11_DISPLAY>" --)
elseif(_wrapper_virtual_display_provider STREQUAL "xvfb-run")
    list(APPEND _expected_wrapper_command xvfb-run -a)
endif()
list(APPEND _expected_wrapper_command
    ModelDActualWrapperSmoke
    --plugin "${_expected_wrapper_plugin_path}"
    --repeat "${SYNTH_REPEAT_COUNT}"
    --seed "${SYNTH_SEED}"
    --report validation/actual-wrapper-smoke-report.json
    --log validation/actual-wrapper-smoke.log
    --config "${SYNTH_CONFIGURATION}"
    --os "${SYNTH_SYSTEM_NAME}"
    --arch "${SYNTH_ARCHITECTURE}")
set(_wrapper_command "")
string(JSON _wrapper_command_count LENGTH "${_wrapper}" command)
if(_wrapper_command_count GREATER 0)
    math(EXPR _last_wrapper_command_token "${_wrapper_command_count} - 1")
    foreach(_wrapper_command_index RANGE 0 ${_last_wrapper_command_token})
        string(JSON _wrapper_command_token GET "${_wrapper}"
               command ${_wrapper_command_index})
        list(APPEND _wrapper_command "${_wrapper_command_token}")
    endforeach()
endif()
if(NOT _wrapper_command STREQUAL _expected_wrapper_command)
    message(FATAL_ERROR "actual-wrapper normalized launch command is inconsistent")
endif()
if(NOT _wrapper_status STREQUAL "pass"
   OR NOT _wrapper_repeat_count STREQUAL SYNTH_REPEAT_COUNT
   OR NOT _wrapper_seed STREQUAL SYNTH_SEED
   OR NOT _wrapper_plugin_path STREQUAL _expected_wrapper_plugin_path
   OR NOT _wrapper_plugin_sha256 STREQUAL _final_vst3_sha256
   OR NOT _wrapper_os STREQUAL _build_os
   OR NOT _wrapper_architecture STREQUAL _build_architecture
   OR NOT _wrapper_configuration STREQUAL _build_configuration)
    message(FATAL_ERROR "actual-wrapper report does not satisfy the repeat/seed contract")
endif()
string(JSON _auval_status GET "${_auval}" status)
string(JSON _auval_os GET "${_auval}" os)
string(JSON _auval_architecture GET "${_auval}" architecture)
string(JSON _auval_configuration GET "${_auval}" configuration)
_synth_require_status("${_auval_status}" "auval report")
if(NOT _auval_os STREQUAL _build_os
   OR NOT _auval_architecture STREQUAL _build_architecture
   OR NOT _auval_configuration STREQUAL _build_configuration)
    message(FATAL_ERROR "auval report environment differs from the final build manifest")
endif()
string(JSON _vst3_validator_status GET "${_pluginval}" vst3_sdk_validator status)
string(JSON _vst3_validator_json GET "${_pluginval}" vst3_sdk_validator)
_synth_require_status("${_vst3_validator_status}" "VST3 SDK validator report")

# Build an exact cryptographic evidence inventory and require Task 6A to have
# declared every expected validator file before this adjacent metadata is made.
set(_build_evidence_paths "")
set(_evidence_json "[]")
string(JSON _report_count LENGTH "${_build_manifest}" test_reports)
if(_report_count GREATER 0)
    math(EXPR _last_report "${_report_count} - 1")
    foreach(_report_index RANGE 0 ${_last_report})
        string(JSON _report_entry GET "${_build_manifest}" test_reports ${_report_index})
        string(JSON _relative_path GET "${_report_entry}" path)
        string(JSON _declared_sha256 GET "${_report_entry}" sha256)
        if(_relative_path MATCHES "\\\\"
           OR IS_ABSOLUTE "${_relative_path}"
           OR _relative_path MATCHES "(^|/)[.][.](/|$)")
            message(FATAL_ERROR "build manifest contains unsafe evidence path: ${_relative_path}")
        endif()
        if(NOT EXISTS "${_build_root}/${_relative_path}")
            message(FATAL_ERROR "declared validation evidence is missing: ${_relative_path}")
        endif()
        file(SHA256 "${_build_root}/${_relative_path}" _actual_sha256)
        if(NOT _actual_sha256 STREQUAL _declared_sha256)
            message(FATAL_ERROR "declared validation evidence hash mismatch: ${_relative_path}")
        endif()
        list(APPEND _build_evidence_paths "${_relative_path}")
        string(JSON _evidence_json SET "${_evidence_json}" ${_report_index} "${_report_entry}")
    endforeach()
endif()
set(_expected_evidence ${SYNTH_EXPECTED_EVIDENCE})
list(SORT _expected_evidence)
list(SORT _build_evidence_paths)
if(NOT _expected_evidence STREQUAL _build_evidence_paths)
    message(FATAL_ERROR
        "build manifest validation evidence set is incomplete or undeclared; expected '${_expected_evidence}', got '${_build_evidence_paths}'")
endif()

file(SHA256 "${SYNTH_WRAPPER_REPORT_PATH}" _wrapper_report_sha256)
file(SHA256 "${SYNTH_WRAPPER_LOG_PATH}" _wrapper_log_sha256)
file(RELATIVE_PATH _wrapper_report_relative "${_build_root_lexical}" "${SYNTH_WRAPPER_REPORT_PATH}")
file(RELATIVE_PATH _wrapper_log_relative "${_build_root_lexical}" "${SYNTH_WRAPPER_LOG_PATH}")
string(REPLACE "\\" "/" _wrapper_report_relative "${_wrapper_report_relative}")
string(REPLACE "\\" "/" _wrapper_log_relative "${_wrapper_log_relative}")
set(_wrapper_summary "{}")
_synth_json_set_string(_wrapper_summary status "pass")
_synth_json_set_string(_wrapper_summary report_path "${_wrapper_report_relative}")
_synth_json_set_string(_wrapper_summary report_sha256 "${_wrapper_report_sha256}")
_synth_json_set_string(_wrapper_summary log_path "${_wrapper_log_relative}")
_synth_json_set_string(_wrapper_summary log_sha256 "${_wrapper_log_sha256}")

string(JSON _host_schema GET "${_host_definition}" schema_version)
string(JSON _host_row_count LENGTH "${_host_definition}" rows)
string(JSON _host_check_count LENGTH "${_host_definition}" checks)
if(NOT _host_schema STREQUAL "1" OR NOT _host_row_count EQUAL 11 OR NOT _host_check_count EQUAL 9)
    message(FATAL_ERROR "required host matrix must contain schema 1, 11 rows, and 9 checks")
endif()
file(SHA256 "${SYNTH_HOST_MATRIX_PATH}" _host_definition_sha256)
set(_host_results "[]")
math(EXPR _last_host_row "${_host_row_count} - 1")
foreach(_row_index RANGE 0 ${_last_host_row})
    string(JSON _definition_row GET "${_host_definition}" rows ${_row_index})
    string(JSON _host_name GET "${_definition_row}" name)
    string(JSON _required_version GET "${_definition_row}" required_version)
    string(JSON _platform GET "${_definition_row}" platform)
    string(JSON _source_url GET "${_definition_row}" source_url)
    string(JSON _kind GET "${_definition_row}" kind)
    if(_required_version STREQUAL "\${PROJECT_VERSION}")
        set(_required_version "${SYNTH_PROJECT_VERSION}")
    endif()
    set(_row "{}")
    _synth_json_set_string(_row name "${_host_name}")
    _synth_json_set_string(_row required_version "${_required_version}")
    _synth_json_set_string(_row platform "${_platform}")
    _synth_json_set_string(_row source_url "${_source_url}")
    _synth_json_set_string(_row kind "${_kind}")
    _synth_json_set_string(_row installed_version "")
    _synth_json_set_string(_row executed_os "")
    _synth_json_set_string(_row executed_architecture "")
    if(_kind STREQUAL "standalone")
        _synth_json_set_string(_row status "not-run")
        _synth_json_set_string(_row reason "BLD-009 lifecycle evidence does not cover the full standalone host matrix")
    else()
        _synth_json_set_string(_row status "blocked")
        _synth_json_set_string(_row reason "Commercial host is unavailable and was not executed by this local harness")
    endif()
    set(_checks "[]")
    math(EXPR _last_check "${_host_check_count} - 1")
    foreach(_check_index RANGE 0 ${_last_check})
        string(JSON _check_name GET "${_host_definition}" checks ${_check_index})
        set(_check "{}")
        _synth_json_set_string(_check name "${_check_name}")
        _synth_json_set_string(_check status "not-run")
        string(JSON _checks SET "${_checks}" ${_check_index} "${_check}")
    endforeach()
    string(JSON _row SET "${_row}" checks "${_checks}")
    string(JSON _host_results SET "${_host_results}" ${_row_index} "${_row}")
endforeach()
set(_hosts "{}")
_synth_json_set_string(_hosts definition_path "validation/required-host-matrix.json")
_synth_json_set_string(_hosts definition_sha256 "${_host_definition_sha256}")
string(JSON _hosts SET "${_hosts}" rows "${_host_results}")

set(_build_reference "{}")
_synth_json_set_string(_build_reference path "build-manifest.json")
_synth_json_set_string(_build_reference sha256 "${_build_manifest_sha256}")
set(_environment "{}")
_synth_json_set_string(_environment os "${SYNTH_SYSTEM_NAME}")
_synth_json_set_string(_environment architecture "${SYNTH_ARCHITECTURE}")
_synth_json_set_string(_environment configuration "${SYNTH_CONFIGURATION}")

set(_validation_reasons "[]")
set(_reason_index 0)
if(NOT _vst3_validator_status STREQUAL "pass")
    _synth_json_quote(_reason "Steinberg VST3 SDK validator is ${_vst3_validator_status}")
    string(JSON _validation_reasons SET "${_validation_reasons}" ${_reason_index} "${_reason}")
    math(EXPR _reason_index "${_reason_index} + 1")
endif()
if(NOT _auval_status STREQUAL "pass")
    _synth_json_quote(_reason "auval is ${_auval_status}")
    string(JSON _validation_reasons SET "${_validation_reasons}" ${_reason_index} "${_reason}")
    math(EXPR _reason_index "${_reason_index} + 1")
endif()
_synth_json_quote(_reason "Required commercial-host and standalone host-matrix rows remain blocked/not-run")
string(JSON _validation_reasons SET "${_validation_reasons}" ${_reason_index} "${_reason}")
set(_validation_aggregate "{}")
_synth_json_set_string(_validation_aggregate status "blocked")
string(JSON _validation_aggregate SET "${_validation_aggregate}" reasons "${_validation_reasons}")
set(_identity_aggregate "{}")
set(_identity_reasons "[]")
string(JSON _identity_approved GET "${_build_manifest}" identity identity_approved)
string(JSON _identity_manufacturer GET "${_build_manifest}" identity manufacturer_name)
string(JSON _identity_domain GET "${_build_manifest}" identity manufacturer_domain)
if(_identity_approved
   AND NOT _identity_manufacturer STREQUAL ""
   AND NOT _identity_manufacturer STREQUAL "yourcompany"
   AND NOT _identity_domain MATCHES "(^|\\.)yourcompany($|\\.)")
    _synth_json_set_string(_identity_aggregate status "pass")
else()
    _synth_json_set_string(_identity_aggregate status "blocked")
    _synth_json_quote(_identity_reason "Product identity is unapproved/placeholder (BLD-007)")
    string(JSON _identity_reasons SET "${_identity_reasons}" 0 "${_identity_reason}")
endif()
string(JSON _identity_aggregate SET "${_identity_aggregate}" reasons "${_identity_reasons}")
set(_distribution_aggregate "{}")
_synth_json_set_string(_distribution_aggregate status "blocked")
set(_distribution_reasons "[]")
_synth_json_quote(_distribution_reason "Identity and mandatory validation gates are unresolved")
string(JSON _distribution_reasons SET "${_distribution_reasons}" 0 "${_distribution_reason}")
string(JSON _distribution_aggregate SET "${_distribution_aggregate}" reasons "${_distribution_reasons}")
set(_release_aggregate "{}")
_synth_json_set_string(_release_aggregate status "blocked")
set(_release_reasons "[]")
_synth_json_quote(_release_reason "Distribution and required host validation are blocked")
string(JSON _release_reasons SET "${_release_reasons}" 0 "${_release_reason}")
string(JSON _release_aggregate SET "${_release_aggregate}" reasons "${_release_reasons}")
set(_aggregates "{}")
string(JSON _aggregates SET "${_aggregates}" validation "${_validation_aggregate}")
string(JSON _aggregates SET "${_aggregates}" identity "${_identity_aggregate}")
string(JSON _aggregates SET "${_aggregates}" distribution "${_distribution_aggregate}")
string(JSON _aggregates SET "${_aggregates}" release "${_release_aggregate}")

set(_manifest "{}")
string(JSON _manifest SET "${_manifest}" schema_version 1)
string(JSON _manifest SET "${_manifest}" build_manifest "${_build_reference}")
string(JSON _manifest SET "${_manifest}" environment "${_environment}")
string(JSON _manifest SET "${_manifest}" repeat_count ${SYNTH_REPEAT_COUNT})
_synth_json_set_string(_manifest seed "${SYNTH_SEED}")
string(JSON _manifest SET "${_manifest}" actual_wrapper "${_wrapper_summary}")
set(_pluginval_tool "{}")
string(JSON _pluginval_tool SET "${_pluginval_tool}" provision "${_provision}")
string(JSON _pluginval_tool SET "${_pluginval_tool}" run "${_pluginval}")
string(JSON _manifest SET "${_manifest}" pluginval "${_pluginval_tool}")
string(JSON _manifest SET "${_manifest}" auval "${_auval}")
string(JSON _manifest SET "${_manifest}" standalone_lifecycle "${_standalone}")
string(JSON _manifest SET "${_manifest}" vst3_sdk_validator "${_vst3_validator_json}")
string(JSON _manifest SET "${_manifest}" hosts "${_hosts}")
string(JSON _manifest SET "${_manifest}" evidence "${_evidence_json}")
string(JSON _manifest SET "${_manifest}" aggregates "${_aggregates}")
file(WRITE "${_validation_manifest_path}" "${_manifest}\n")
message(STATUS "Generated linked validation manifest (release status: blocked)")
