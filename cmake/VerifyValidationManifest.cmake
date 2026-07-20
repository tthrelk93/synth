cmake_minimum_required(VERSION 3.24)

if(DEFINED SYNTH_EXPECTED_EVIDENCE_FILE)
    if("${SYNTH_EXPECTED_EVIDENCE_FILE}" STREQUAL ""
       OR NOT EXISTS "${SYNTH_EXPECTED_EVIDENCE_FILE}")
        message(FATAL_ERROR
            "SYNTH_EXPECTED_EVIDENCE_FILE must name an existing inventory")
    endif()
    file(STRINGS "${SYNTH_EXPECTED_EVIDENCE_FILE}" SYNTH_EXPECTED_EVIDENCE
         ENCODING UTF-8)
    if(NOT SYNTH_EXPECTED_EVIDENCE)
        message(FATAL_ERROR
            "SYNTH_EXPECTED_EVIDENCE_FILE inventory is empty")
    endif()
elseif(DEFINED SYNTH_EXPECTED_EVIDENCE)
    string(REPLACE "\\;" ";" SYNTH_EXPECTED_EVIDENCE
                   "${SYNTH_EXPECTED_EVIDENCE}")
endif()

foreach(_required_variable IN ITEMS
        SYNTH_BUILD_ROOT
        SYNTH_STAGE_DIRECTORY
        SYNTH_HOST_MATRIX_PATH
        SYNTH_REPEAT_COUNT
        SYNTH_SEED
        SYNTH_PRODUCT_CODE
        SYNTH_MANUFACTURER_CODE
        SYNTH_PROJECT_VERSION
        SYNTH_SYSTEM_NAME
        SYNTH_ARCHITECTURE
        SYNTH_CONFIGURATION
        SYNTH_EXPECTED_EVIDENCE)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required to verify validation metadata")
    endif()
endforeach()

set(_expected_pluginval_environment_variables
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
list(JOIN _expected_pluginval_environment_variables ","
     _expected_pluginval_environment_record)

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

function(_synth_require_same_json left_json right_json context)
    string(JSON _left_normalized SET "{}" value "${left_json}")
    string(JSON _right_normalized SET "{}" value "${right_json}")
    if(NOT _left_normalized STREQUAL _right_normalized)
        message(FATAL_ERROR "${context} differs from its hashed evidence report")
    endif()
endfunction()

function(_synth_verify_build_evidence relative_path declared_sha256 description)
    string(REPLACE "\\" "/" _relative_path "${relative_path}")
    if(_relative_path STREQUAL ""
       OR IS_ABSOLUTE "${_relative_path}"
       OR _relative_path MATCHES "^[A-Za-z]:"
       OR _relative_path MATCHES "(^|/)[.][.](/|$)")
        message(FATAL_ERROR "${description} has an unsafe path: ${relative_path}")
    endif()
    if(NOT EXISTS "${_build_root}/${_relative_path}")
        message(FATAL_ERROR "${description} is missing: ${_relative_path}")
    endif()
    file(REAL_PATH "${_build_root}/${_relative_path}" _evidence_real)
    file(RELATIVE_PATH _from_build "${_build_root}" "${_evidence_real}")
    string(REPLACE "\\" "/" _from_build "${_from_build}")
    if(IS_ABSOLUTE "${_from_build}" OR _from_build MATCHES "(^|/)[.][.](/|$)")
        message(FATAL_ERROR "${description} escapes the build root: ${relative_path}")
    endif()
    if(IS_DIRECTORY "${_evidence_real}")
        message(FATAL_ERROR "${description} is a directory: ${relative_path}")
    endif()
    file(SHA256 "${_evidence_real}" _actual_sha256)
    if(NOT _actual_sha256 STREQUAL declared_sha256)
        message(FATAL_ERROR "${description} hash mismatch: ${relative_path}")
    endif()
endfunction()

file(REAL_PATH "${SYNTH_BUILD_ROOT}" _build_root)
file(REAL_PATH "${SYNTH_STAGE_DIRECTORY}" _stage_root)
set(_build_manifest_path "${_stage_root}/build-manifest.json")
set(_validation_manifest_path "${_stage_root}/validation-manifest.json")
_synth_read_json(_build_manifest "${_build_manifest_path}" "build manifest")
_synth_read_json(_validation "${_validation_manifest_path}" "validation manifest")

string(JSON _schema GET "${_validation}" schema_version)
if(NOT _schema STREQUAL "1")
    message(FATAL_ERROR "unsupported validation manifest schema: ${_schema}")
endif()
file(SHA256 "${_build_manifest_path}" _actual_build_manifest_sha256)
string(JSON _build_manifest_relative GET "${_validation}" build_manifest path)
string(JSON _declared_build_manifest_sha256 GET "${_validation}" build_manifest sha256)
if(NOT _build_manifest_relative STREQUAL "build-manifest.json"
   OR NOT _declared_build_manifest_sha256 STREQUAL _actual_build_manifest_sha256)
    message(FATAL_ERROR "validation manifest is not linked to the current build manifest")
endif()

string(JSON _build_os GET "${_build_manifest}" build os)
string(JSON _build_architecture GET "${_build_manifest}" build architecture)
string(JSON _build_configuration GET "${_build_manifest}" build configuration)
string(JSON _validation_os GET "${_validation}" environment os)
string(JSON _validation_architecture GET "${_validation}" environment architecture)
string(JSON _validation_configuration GET "${_validation}" environment configuration)
if(NOT _build_os STREQUAL SYNTH_SYSTEM_NAME
   OR NOT _validation_os STREQUAL SYNTH_SYSTEM_NAME
   OR NOT _build_architecture STREQUAL SYNTH_ARCHITECTURE
   OR NOT _validation_architecture STREQUAL SYNTH_ARCHITECTURE
   OR NOT _build_configuration STREQUAL SYNTH_CONFIGURATION
   OR NOT _validation_configuration STREQUAL SYNTH_CONFIGURATION)
    message(FATAL_ERROR
        "validation environment does not match the final build manifest/configured build")
endif()

string(JSON _repeat_count GET "${_validation}" repeat_count)
string(JSON _seed GET "${_validation}" seed)
if(NOT _repeat_count STREQUAL SYNTH_REPEAT_COUNT OR NOT _seed STREQUAL SYNTH_SEED)
    message(FATAL_ERROR "validation repeat count/seed does not match the configured contract")
endif()

string(JSON _product_count LENGTH "${_build_manifest}" products)
set(_vst3_count 0)
if(_product_count GREATER 0)
    math(EXPR _last_product "${_product_count} - 1")
    foreach(_product_index RANGE 0 ${_last_product})
        string(JSON _format GET "${_build_manifest}" products ${_product_index} format)
        if(_format STREQUAL "VST3")
            math(EXPR _vst3_count "${_vst3_count} + 1")
            string(JSON _final_vst3_relative_path GET "${_build_manifest}" products ${_product_index} relative_path)
            string(JSON _final_vst3_sha256 GET "${_build_manifest}" products ${_product_index} aggregate_sha256)
        endif()
    endforeach()
endif()
if(NOT _vst3_count EQUAL 1)
    message(FATAL_ERROR "final build manifest must declare exactly one VST3 product")
endif()

# The build and validation manifests must declare the exact same report set,
# and that set must equal the fixed set constructed by the build graph.
set(_build_paths "")
set(_build_hashes "")
string(JSON _build_report_count LENGTH "${_build_manifest}" test_reports)
if(_build_report_count GREATER 0)
    math(EXPR _last_build_report "${_build_report_count} - 1")
    foreach(_index RANGE 0 ${_last_build_report})
        string(JSON _path GET "${_build_manifest}" test_reports ${_index} path)
        string(JSON _sha256 GET "${_build_manifest}" test_reports ${_index} sha256)
        list(FIND _build_paths "${_path}" _duplicate)
        if(NOT _duplicate EQUAL -1)
            message(FATAL_ERROR "build manifest contains duplicate evidence: ${_path}")
        endif()
        _synth_verify_build_evidence("${_path}" "${_sha256}" "build-manifest evidence")
        list(APPEND _build_paths "${_path}")
        list(APPEND _build_hashes "${_sha256}")
    endforeach()
endif()
set(_validation_paths "")
string(JSON _validation_evidence_count LENGTH "${_validation}" evidence)
if(_validation_evidence_count GREATER 0)
    math(EXPR _last_validation_evidence "${_validation_evidence_count} - 1")
    foreach(_index RANGE 0 ${_last_validation_evidence})
        string(JSON _path GET "${_validation}" evidence ${_index} path)
        string(JSON _sha256 GET "${_validation}" evidence ${_index} sha256)
        list(FIND _validation_paths "${_path}" _duplicate)
        if(NOT _duplicate EQUAL -1)
            message(FATAL_ERROR "validation manifest contains duplicate evidence: ${_path}")
        endif()
        list(FIND _build_paths "${_path}" _build_index)
        if(_build_index EQUAL -1)
            message(FATAL_ERROR "validation manifest contains undeclared evidence: ${_path}")
        endif()
        list(GET _build_hashes ${_build_index} _build_sha256)
        if(NOT _sha256 STREQUAL _build_sha256)
            message(FATAL_ERROR "manifests disagree on evidence hash: ${_path}")
        endif()
        _synth_verify_build_evidence("${_path}" "${_sha256}" "validation evidence")
        list(APPEND _validation_paths "${_path}")
    endforeach()
endif()
set(_expected_paths ${SYNTH_EXPECTED_EVIDENCE})
list(SORT _expected_paths)
list(SORT _build_paths)
list(SORT _validation_paths)
if(NOT _build_paths STREQUAL _expected_paths OR NOT _validation_paths STREQUAL _expected_paths)
    message(FATAL_ERROR "missing or undeclared validation evidence in linked manifests")
endif()

# Declared evidence is necessary but not sufficient: the fixed validation root
# must not contain stale or rogue files omitted from both linked manifests.
set(_expected_validation_files "")
foreach(_expected_path IN LISTS _expected_paths)
    if(_expected_path MATCHES "^validation/")
        list(APPEND _expected_validation_files "${_expected_path}")
    endif()
endforeach()
file(GLOB_RECURSE _actual_validation_absolute_files
     LIST_DIRECTORIES false "${_build_root}/validation/*")
set(_actual_validation_files "")
foreach(_actual_validation_file IN LISTS _actual_validation_absolute_files)
    if(IS_SYMLINK "${_actual_validation_file}")
        message(FATAL_ERROR
            "validation evidence contains a prohibited symlink: ${_actual_validation_file}")
    endif()
    file(RELATIVE_PATH _actual_validation_relative
         "${_build_root}" "${_actual_validation_file}")
    string(REPLACE "\\" "/" _actual_validation_relative
                   "${_actual_validation_relative}")
    list(APPEND _actual_validation_files "${_actual_validation_relative}")
endforeach()
list(SORT _expected_validation_files)
list(SORT _actual_validation_files)
if(NOT _actual_validation_files STREQUAL _expected_validation_files)
    message(FATAL_ERROR
        "build-owned validation root contains stale, rogue, or missing files; expected '${_expected_validation_files}', got '${_actual_validation_files}'")
endif()

set(_pluginval_evidence_marker
    "${_build_root}/validation-ownership/pluginval-evidence-root.marker")
if(NOT EXISTS "${_pluginval_evidence_marker}")
    message(FATAL_ERROR "pluginval evidence ownership marker is missing")
endif()
file(READ "${_pluginval_evidence_marker}" _pluginval_evidence_marker_contents)
if(NOT _pluginval_evidence_marker_contents STREQUAL
       "model-d-pluginval-evidence-root-v1\n")
    message(FATAL_ERROR "pluginval evidence ownership marker is invalid")
endif()

_synth_read_json(_provision_report
    "${_build_root}/validation/pluginval-provision.json" "pluginval provision report")
_synth_read_json(_pluginval_report
    "${_build_root}/validation/pluginval-run-report.json" "pluginval run report")
_synth_read_json(_auval_report
    "${_build_root}/validation/auval-report.json" "auval report")
_synth_read_json(_standalone_report
    "${_build_root}/validation/standalone-lifecycle-report.json"
    "standalone lifecycle report")
string(JSON _embedded_provision GET "${_validation}" pluginval provision)
string(JSON _embedded_pluginval GET "${_validation}" pluginval run)
string(JSON _embedded_auval GET "${_validation}" auval)
string(JSON _embedded_standalone GET "${_validation}" standalone_lifecycle)
_synth_require_same_json("${_embedded_provision}" "${_provision_report}"
                         "embedded pluginval provision metadata")
_synth_require_same_json("${_embedded_pluginval}" "${_pluginval_report}"
                         "embedded pluginval run metadata")
_synth_require_same_json("${_embedded_auval}" "${_auval_report}"
                         "embedded auval metadata")
_synth_require_same_json("${_embedded_standalone}" "${_standalone_report}"
                         "embedded standalone lifecycle metadata")

set(_standalone_evidence_marker
    "${_build_root}/validation-ownership/standalone-evidence-root.marker")
if(NOT EXISTS "${_standalone_evidence_marker}")
    message(FATAL_ERROR "standalone evidence ownership marker is missing")
endif()
file(READ "${_standalone_evidence_marker}" _standalone_evidence_marker_contents)
if(NOT _standalone_evidence_marker_contents STREQUAL
       "model-d-standalone-evidence-root-v1\n")
    message(FATAL_ERROR "standalone evidence ownership marker is invalid")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSYNTH_BUILD_ROOT=${_build_root}"
        "-DSYNTH_STAGE_DIRECTORY=${_stage_root}"
        "-DSYNTH_STANDALONE_REPORT_PATH=${_build_root}/validation/standalone-lifecycle-report.json"
        "-DSYNTH_REPEAT_COUNT=${SYNTH_REPEAT_COUNT}"
        "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
        "-DSYNTH_ARCHITECTURE=${SYNTH_ARCHITECTURE}"
        "-DSYNTH_CONFIGURATION=${SYNTH_CONFIGURATION}"
        "-DSYNTH_PROJECT_VERSION=${SYNTH_PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_LIST_DIR}/VerifyStandaloneLifecycleEvidence.cmake"
    RESULT_VARIABLE _standalone_verification_status
    OUTPUT_VARIABLE _standalone_verification_output
    ERROR_VARIABLE _standalone_verification_error
    ENCODING UTF-8)
if(NOT "${_standalone_verification_status}" MATCHES "^[0-9]+$"
   OR NOT "${_standalone_verification_status}" STREQUAL "0")
    message(FATAL_ERROR
        "linked standalone lifecycle evidence failed per-run verification:\n${_standalone_verification_output}${_standalone_verification_error}")
endif()

string(JSON _wrapper_status GET "${_validation}" actual_wrapper status)
string(JSON _wrapper_report_path GET "${_validation}" actual_wrapper report_path)
string(JSON _wrapper_report_sha256 GET "${_validation}" actual_wrapper report_sha256)
string(JSON _wrapper_log_path GET "${_validation}" actual_wrapper log_path)
string(JSON _wrapper_log_sha256 GET "${_validation}" actual_wrapper log_sha256)
if(NOT _wrapper_status STREQUAL "pass")
    message(FATAL_ERROR "actual-wrapper aggregate is not pass")
endif()
_synth_verify_build_evidence("${_wrapper_report_path}" "${_wrapper_report_sha256}" "wrapper report")
_synth_verify_build_evidence("${_wrapper_log_path}" "${_wrapper_log_sha256}" "wrapper log")
_synth_read_json(_wrapper_report "${_build_root}/${_wrapper_report_path}" "wrapper report")
string(JSON _wrapper_report_status GET "${_wrapper_report}" status)
string(JSON _wrapper_repeat_count GET "${_wrapper_report}" repeat_count)
string(JSON _wrapper_seed GET "${_wrapper_report}" seed)
string(JSON _wrapper_plugin_path GET "${_wrapper_report}" plugin relative_path)
string(JSON _wrapper_plugin_sha256 GET "${_wrapper_report}" plugin aggregate_sha256)
string(JSON _wrapper_os GET "${_wrapper_report}" build os)
string(JSON _wrapper_architecture GET "${_wrapper_report}" build architecture)
string(JSON _wrapper_configuration GET "${_wrapper_report}" build configuration)
string(JSON _wrapper_virtual_display_provider GET "${_wrapper_report}" build virtual_display_provider)
if(_build_os STREQUAL "Linux")
    if(NOT _wrapper_virtual_display_provider MATCHES "^(inherited-x11|xvfb-run)$")
        message(FATAL_ERROR "wrapper report has an invalid Linux virtual-display provider")
    endif()
elseif(NOT _wrapper_virtual_display_provider STREQUAL "native")
    message(FATAL_ERROR "wrapper report has an invalid native display provider")
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
string(JSON _wrapper_command_count LENGTH "${_wrapper_report}" command)
if(_wrapper_command_count GREATER 0)
    math(EXPR _last_wrapper_command_token "${_wrapper_command_count} - 1")
    foreach(_wrapper_command_index RANGE 0 ${_last_wrapper_command_token})
        string(JSON _wrapper_command_token GET "${_wrapper_report}"
               command ${_wrapper_command_index})
        list(APPEND _wrapper_command "${_wrapper_command_token}")
    endforeach()
endif()
if(NOT _wrapper_command STREQUAL _expected_wrapper_command)
    message(FATAL_ERROR "wrapper report normalized launch command is inconsistent")
endif()
if(NOT _wrapper_report_status STREQUAL "pass"
   OR NOT _wrapper_repeat_count STREQUAL SYNTH_REPEAT_COUNT
   OR NOT _wrapper_seed STREQUAL SYNTH_SEED
   OR NOT _wrapper_plugin_path STREQUAL _expected_wrapper_plugin_path
   OR NOT _wrapper_plugin_sha256 STREQUAL _final_vst3_sha256
   OR NOT _wrapper_os STREQUAL _build_os
   OR NOT _wrapper_architecture STREQUAL _build_architecture
   OR NOT _wrapper_configuration STREQUAL _build_configuration)
    message(FATAL_ERROR "wrapper report is inconsistent with validation metadata")
endif()

string(JSON _provision_version GET "${_validation}" pluginval provision version)
string(JSON _provision_version_output GET "${_validation}" pluginval provision version_output)
string(JSON _provision_schema GET "${_validation}" pluginval provision schema_version)
string(JSON _provision_name GET "${_validation}" pluginval provision name)
string(JSON _provision_os GET "${_validation}" pluginval provision os)
string(JSON _platform_asset GET "${_validation}" pluginval provision platform_asset)
string(JSON _download_url GET "${_validation}" pluginval provision download_url)
string(JSON _executable_relative_path GET "${_validation}" pluginval provision executable_relative_path)
string(JSON _archive_status GET "${_validation}" pluginval provision archive_status)
string(JSON _archive_sha256 GET "${_validation}" pluginval provision archive_sha256)
string(JSON _expected_archive_sha256 GET "${_validation}" pluginval provision expected_archive_sha256)
string(JSON _archive_source GET "${_validation}" pluginval provision archive_source)
string(JSON _executable_source GET "${_validation}" pluginval provision executable_source)
string(JSON _provision_executable GET "${_validation}" pluginval provision executable)
string(JSON _executable_sha256 GET "${_validation}" pluginval provision executable_sha256)
string(JSON _provision_environment_sanitized GET "${_validation}" pluginval provision environment_sanitized)
_synth_require_status("${_archive_status}" "pluginval archive")
if(_validation_os STREQUAL "Darwin")
    set(_official_asset "pluginval_macOS.zip")
    set(_official_archive_sha256
        "3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f")
    set(_official_executable_relative_path
        "pluginval.app/Contents/MacOS/pluginval")
elseif(_validation_os STREQUAL "Windows")
    set(_official_asset "pluginval_Windows.zip")
    set(_official_archive_sha256
        "c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab")
    set(_official_executable_relative_path "pluginval.exe")
elseif(_validation_os STREQUAL "Linux")
    set(_official_asset "pluginval_Linux.zip")
    set(_official_archive_sha256
        "c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352")
    set(_official_executable_relative_path "pluginval")
else()
    message(FATAL_ERROR "unsupported pluginval validation OS: ${_validation_os}")
endif()
set(_official_download_url
    "https://github.com/Tracktion/pluginval/releases/download/v1.0.4/${_official_asset}")
if(NOT _provision_schema STREQUAL "1"
   OR NOT _provision_name STREQUAL "pluginval"
   OR NOT _provision_version STREQUAL "1.0.4"
   OR NOT _provision_version_output STREQUAL "pluginval - 1.0.4"
   OR NOT _provision_os STREQUAL _validation_os
   OR NOT _platform_asset STREQUAL _official_asset
   OR NOT _download_url STREQUAL _official_download_url
   OR NOT _executable_relative_path STREQUAL _official_executable_relative_path
   OR NOT _expected_archive_sha256 STREQUAL _official_archive_sha256
   OR NOT _provision_environment_sanitized
   OR _executable_sha256 STREQUAL "")
    message(FATAL_ERROR "pluginval provisioning provenance is inconsistent")
endif()
set(_provision_unset_environment_variables "")
string(JSON _provision_unset_count LENGTH "${_validation}"
       pluginval provision unset_environment_variables)
if(_provision_unset_count GREATER 0)
    math(EXPR _last_provision_unset "${_provision_unset_count} - 1")
    foreach(_index RANGE 0 ${_last_provision_unset})
        string(JSON _environment_variable GET "${_validation}"
               pluginval provision unset_environment_variables ${_index})
        list(APPEND _provision_unset_environment_variables
             "${_environment_variable}")
    endforeach()
endif()
if(NOT _provision_unset_environment_variables STREQUAL
       _expected_pluginval_environment_variables)
    message(FATAL_ERROR "pluginval provision environment sanitation is incomplete")
endif()
if(_archive_status STREQUAL "pass")
    if(NOT _archive_sha256 STREQUAL _official_archive_sha256
       OR NOT _archive_source MATCHES "^(official-download|explicit-archive-override)$"
       OR NOT _executable_source STREQUAL "pinned-official-archive")
        message(FATAL_ERROR "pluginval archive provenance is inconsistent")
    endif()
    set(_expected_provision_executable
        "validation-tools/pluginval/v1.0.4/extracted/${_official_executable_relative_path}")
    if(NOT _provision_executable STREQUAL _expected_provision_executable)
        message(FATAL_ERROR "pluginval provisioned executable path is inconsistent")
    endif()
    _synth_verify_build_evidence("${_provision_executable}"
                                 "${_executable_sha256}"
                                 "provisioned pluginval executable")
elseif(_archive_status STREQUAL "not-run")
    if(NOT _archive_sha256 STREQUAL ""
       OR NOT _archive_source STREQUAL "not-run-executable-override"
       OR NOT _executable_source STREQUAL "explicit-executable-override"
       OR NOT _provision_executable STREQUAL "external-override")
        message(FATAL_ERROR "pluginval executable-override provenance is inconsistent")
    endif()
else()
    message(FATAL_ERROR "pluginval archive status must be pass or not-run")
endif()
set(_pluginval_tool_root
    "${_build_root}/validation-tools/pluginval/v1.0.4")
set(_pluginval_tool_marker
    "${_pluginval_tool_root}/.synth-pluginval-tool-root")
set(_pluginval_stamp "${_pluginval_tool_root}/provision.stamp")
foreach(_required_tool_control IN ITEMS
        "${_pluginval_tool_marker}" "${_pluginval_stamp}")
    if(NOT EXISTS "${_required_tool_control}")
        message(FATAL_ERROR "pluginval tool control file is missing: ${_required_tool_control}")
    endif()
endforeach()
file(READ "${_pluginval_tool_marker}" _pluginval_tool_marker_contents)
if(NOT _pluginval_tool_marker_contents STREQUAL
       "model-d-pluginval-tool-root-v1\n")
    message(FATAL_ERROR "pluginval tool-root ownership marker is invalid")
endif()
file(READ "${_pluginval_stamp}" _pluginval_stamp_contents)
if(NOT _pluginval_stamp_contents STREQUAL
       "pluginval - 1.0.4\n${_executable_sha256}\n")
    message(FATAL_ERROR "pluginval provision stamp is inconsistent")
endif()

string(JSON _pluginval_status GET "${_validation}" pluginval run status)
string(JSON _pluginval_version GET "${_validation}" pluginval run tool_version)
string(JSON _pluginval_repeat_count GET "${_validation}" pluginval run repeat_count)
string(JSON _pluginval_seed GET "${_validation}" pluginval run seed)
string(JSON _pluginval_strictness GET "${_validation}" pluginval run strictness_level)
string(JSON _pluginval_timeout GET "${_validation}" pluginval run timeout_ms)
string(JSON _pluginval_isolated GET "${_validation}" pluginval run isolated_process)
string(JSON _pluginval_gui GET "${_validation}" pluginval run gui_tests)
string(JSON _pluginval_environment_sanitized GET "${_validation}" pluginval run environment_sanitized)
string(JSON _run_executable_sha256 GET "${_validation}" pluginval run executable_sha256)
string(JSON _pluginval_plugin_path GET "${_validation}" pluginval run plugin_relative_path)
string(JSON _pluginval_plugin_sha256 GET "${_validation}" pluginval run plugin_aggregate_sha256)
string(JSON _pluginval_os GET "${_validation}" pluginval run os)
string(JSON _pluginval_architecture GET "${_validation}" pluginval run architecture)
string(JSON _pluginval_configuration GET "${_validation}" pluginval run configuration)
string(JSON _pluginval_virtual_display_provider GET "${_validation}" pluginval run virtual_display_provider)
if(_build_os STREQUAL "Linux")
    if(NOT _pluginval_virtual_display_provider MATCHES "^(inherited-x11|xvfb-run)$")
        message(FATAL_ERROR "pluginval run has an invalid Linux virtual-display provider")
    endif()
elseif(NOT _pluginval_virtual_display_provider STREQUAL "native")
    message(FATAL_ERROR "pluginval run has an invalid native display provider")
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
   OR NOT _run_executable_sha256 STREQUAL _executable_sha256
   OR NOT _pluginval_plugin_path STREQUAL _final_vst3_relative_path
   OR NOT _pluginval_plugin_sha256 STREQUAL _final_vst3_sha256
   OR NOT _pluginval_os STREQUAL _build_os
   OR NOT _pluginval_architecture STREQUAL _build_architecture
   OR NOT _pluginval_configuration STREQUAL _build_configuration)
    message(FATAL_ERROR "pluginval run contract is inconsistent")
endif()
set(_pluginval_unset_environment_variables "")
string(JSON _pluginval_unset_count LENGTH "${_validation}"
       pluginval run unset_environment_variables)
if(_pluginval_unset_count GREATER 0)
    math(EXPR _last_pluginval_unset "${_pluginval_unset_count} - 1")
    foreach(_index RANGE 0 ${_last_pluginval_unset})
        string(JSON _environment_variable GET "${_validation}"
               pluginval run unset_environment_variables ${_index})
        list(APPEND _pluginval_unset_environment_variables
             "${_environment_variable}")
    endforeach()
endif()
if(NOT _pluginval_unset_environment_variables STREQUAL
       _expected_pluginval_environment_variables)
    message(FATAL_ERROR "pluginval run environment sanitation is incomplete")
endif()
string(JSON _pluginval_repeat_length LENGTH "${_validation}" pluginval run repeats)
if(NOT _pluginval_repeat_length STREQUAL SYNTH_REPEAT_COUNT)
    message(FATAL_ERROR "pluginval did not record the configured number of isolated processes")
endif()
string(JSON _vst3_status GET "${_validation}" vst3_sdk_validator status)
_synth_require_status("${_vst3_status}" "VST3 SDK validator")
math(EXPR _last_pluginval_repeat "${_pluginval_repeat_length} - 1")
foreach(_index RANGE 0 ${_last_pluginval_repeat})
    string(JSON _repeat_status GET "${_validation}" pluginval run repeats ${_index} status)
    string(JSON _report_path GET "${_validation}" pluginval run repeats ${_index} report_path)
    string(JSON _report_sha256 GET "${_validation}" pluginval run repeats ${_index} report_sha256)
    string(JSON _log_path GET "${_validation}" pluginval run repeats ${_index} process_log_path)
    string(JSON _log_sha256 GET "${_validation}" pluginval run repeats ${_index} process_log_sha256)
    if(NOT _repeat_status STREQUAL "pass")
        message(FATAL_ERROR "pluginval repeat ${_index} is not pass")
    endif()
    _synth_verify_build_evidence("${_report_path}" "${_report_sha256}" "pluginval report")
    _synth_verify_build_evidence("${_log_path}" "${_log_sha256}" "pluginval process log")
    math(EXPR _repeat_number "${_index} + 1")
    set(_expected_command cmake -E env)
    foreach(_environment_variable IN LISTS _expected_pluginval_environment_variables)
        list(APPEND _expected_command "--unset=${_environment_variable}")
    endforeach()
    if(_pluginval_virtual_display_provider STREQUAL "inherited-x11")
        list(APPEND _expected_command "DISPLAY=<INHERITED_X11_DISPLAY>")
    endif()
    list(APPEND _expected_command --)
    if(_pluginval_virtual_display_provider STREQUAL "xvfb-run")
        list(APPEND _expected_command xvfb-run -a)
    endif()
    list(APPEND _expected_command
        pluginval
        --validate "${_expected_wrapper_plugin_path}"
        --strictness-level 10
        --random-seed "${SYNTH_SEED}"
        --timeout-ms 60000
        --output-dir "validation/pluginval/repeat-${_repeat_number}"
        --output-filename pluginval-report.txt)
    if(_vst3_status STREQUAL "pass")
        list(APPEND _expected_command --vst3validator configured-vst3-validator)
    endif()
    set(_actual_command "")
    string(JSON _command_length LENGTH "${_validation}" pluginval run repeats ${_index} command)
    if(_command_length GREATER 0)
        math(EXPR _last_command_token "${_command_length} - 1")
        foreach(_command_index RANGE 0 ${_last_command_token})
            string(JSON _command_token GET "${_validation}" pluginval run repeats ${_index} command ${_command_index})
            list(APPEND _actual_command "${_command_token}")
        endforeach()
    endif()
    if(NOT _actual_command STREQUAL _expected_command)
        message(FATAL_ERROR "pluginval repeat ${_repeat_number} exact command/options are inconsistent")
    endif()
    file(READ "${_build_root}/${_log_path}" _process_log)
    foreach(_required_log_line IN ITEMS
            "isolated_process=true"
            "gui_tests=enabled"
            "environment_sanitized=true"
            "unset_environment_variables=${_expected_pluginval_environment_record}"
            "configuration=${_build_configuration}"
            "os=${_build_os}"
            "architecture=${_build_architecture}"
            "strictness_level=10"
            "random_seed=${SYNTH_SEED}"
            "timeout_ms=60000")
        if(NOT _process_log MATCHES "${_required_log_line}")
            message(FATAL_ERROR "pluginval process log omits required option: ${_required_log_line}")
        endif()
    endforeach()
    if(_process_log MATCHES "validate-in-process" OR _process_log MATCHES "skip-gui-tests")
        message(FATAL_ERROR "pluginval process log contains a prohibited option")
    endif()
    file(READ "${_build_root}/${_report_path}" _pluginval_text_report)
    foreach(_required_gui_test IN ITEMS
            "Starting tests in: pluginval / Editor..."
            "Completed tests in pluginval / Editor"
            "Starting tests in: pluginval / Open editor whilst processing..."
            "Completed tests in pluginval / Open editor whilst processing"
            "Starting tests in: pluginval / Editor Automation..."
            "Completed tests in pluginval / Editor Automation")
        string(FIND "${_pluginval_text_report}" "${_required_gui_test}"
               _required_gui_test_position)
        if(_required_gui_test_position EQUAL -1)
            message(FATAL_ERROR
                "pluginval repeat ${_repeat_number} lacks GUI-test evidence: ${_required_gui_test}")
        endif()
    endforeach()
endforeach()

if(_vst3_status STREQUAL "pass")
    string(JSON _vst3_hash GET "${_validation}" vst3_sdk_validator executable_sha256)
    if(_vst3_hash STREQUAL "")
        message(FATAL_ERROR "VST3 SDK validator pass lacks executable provenance")
    endif()
endif()

string(JSON _auval_status GET "${_validation}" auval status)
string(JSON _auval_schema GET "${_validation}" auval schema_version)
string(JSON _auval_tool GET "${_validation}" auval tool)
string(JSON _auval_command GET "${_validation}" auval command)
string(JSON _auval_component_type GET "${_validation}" auval component_type)
string(JSON _auval_product_code GET "${_validation}" auval product_code)
string(JSON _auval_manufacturer_code GET "${_validation}" auval manufacturer_code)
string(JSON _auval_mutated_user_state GET "${_validation}" auval mutated_user_state)
string(JSON _auval_version GET "${_validation}" auval version)
string(JSON _auval_executable_path GET "${_validation}" auval executable_path)
string(JSON _auval_executable_sha256 GET "${_validation}" auval executable_sha256)
string(JSON _auval_exit_code GET "${_validation}" auval exit_code)
string(JSON _auval_reason GET "${_validation}" auval reason)
string(JSON _auval_os GET "${_validation}" auval os)
string(JSON _auval_architecture GET "${_validation}" auval architecture)
string(JSON _auval_configuration GET "${_validation}" auval configuration)
string(JSON _auval_log_path GET "${_validation}" auval log_path)
string(JSON _auval_log_sha256 GET "${_validation}" auval log_sha256)
_synth_require_status("${_auval_status}" "auval")
if(NOT _auval_schema STREQUAL "1"
   OR NOT _auval_tool STREQUAL "auval"
   OR NOT _auval_command STREQUAL "auval -v aumu ${SYNTH_PRODUCT_CODE} ${SYNTH_MANUFACTURER_CODE}"
   OR NOT _auval_component_type STREQUAL "aumu"
   OR NOT _auval_product_code STREQUAL SYNTH_PRODUCT_CODE
   OR NOT _auval_manufacturer_code STREQUAL SYNTH_MANUFACTURER_CODE
   OR _auval_mutated_user_state
   OR NOT _auval_os STREQUAL _build_os
   OR NOT _auval_architecture STREQUAL _build_architecture
   OR NOT _auval_configuration STREQUAL _build_configuration
   OR NOT _auval_log_path STREQUAL "validation/auval.log")
    message(FATAL_ERROR "auval report contract/identity/environment is inconsistent")
endif()
_synth_verify_build_evidence("${_auval_log_path}" "${_auval_log_sha256}" "auval log")
file(READ "${_build_root}/${_auval_log_path}" _auval_log)
foreach(_required_auval_log_line IN ITEMS
        "mutation=none"
        "command=auval -v aumu ${SYNTH_PRODUCT_CODE} ${SYNTH_MANUFACTURER_CODE}"
        "configuration=${_build_configuration}"
        "os=${_build_os}"
        "architecture=${_build_architecture}")
    string(FIND "${_auval_log}" "${_required_auval_log_line}"
           _required_auval_log_position)
    if(_required_auval_log_position EQUAL -1)
        message(FATAL_ERROR "auval log omits provenance: ${_required_auval_log_line}")
    endif()
endforeach()
if(_build_os STREQUAL "Darwin")
    if(_auval_status STREQUAL "pass" OR _auval_status STREQUAL "blocked")
        if(_auval_version STREQUAL ""
           OR _auval_executable_path STREQUAL ""
           OR NOT IS_ABSOLUTE "${_auval_executable_path}"
           OR NOT EXISTS "${_auval_executable_path}"
           OR IS_DIRECTORY "${_auval_executable_path}"
           OR _auval_executable_sha256 STREQUAL "")
            message(FATAL_ERROR "executed auval status lacks coherent executable/version provenance")
        endif()
        file(SHA256 "${_auval_executable_path}" _actual_auval_executable_sha256)
        if(NOT _actual_auval_executable_sha256 STREQUAL _auval_executable_sha256)
            message(FATAL_ERROR "auval executable hash does not match its report")
        endif()
        get_filename_component(_auval_executable_name
                               "${_auval_executable_path}" NAME)
        if(NOT _auval_executable_name STREQUAL "auval")
            message(FATAL_ERROR "auval executable path does not name auval")
        endif()
    elseif(_auval_status STREQUAL "not-run")
        if(NOT _auval_version STREQUAL ""
           OR NOT _auval_executable_path STREQUAL ""
           OR NOT _auval_executable_sha256 STREQUAL ""
           OR NOT _auval_exit_code STREQUAL "not-run")
            message(FATAL_ERROR "unavailable macOS auval report has incoherent provenance")
        endif()
    else()
        message(FATAL_ERROR "persisted macOS auval status must be pass, blocked, or not-run")
    endif()
    if(_auval_status STREQUAL "pass")
        if(NOT _auval_exit_code STREQUAL "0")
            message(FATAL_ERROR "auval pass has a nonzero/non-numeric exit code")
        endif()
    elseif(_auval_status STREQUAL "blocked")
        if(NOT _auval_exit_code MATCHES "^[1-9][0-9]*$"
           OR NOT _auval_reason STREQUAL
               "The AU is not registered in this account; the non-mutating helper did not install it"
           OR NOT _auval_log MATCHES
               "(^|\n)FATAL ERROR: didn't find the component(\r?\n|$)")
            message(FATAL_ERROR "auval blocked status lacks the exact missing-component diagnostic")
        endif()
    endif()
else()
    if(NOT _auval_status STREQUAL "not-run"
       OR NOT _auval_reason STREQUAL "auval is only available on macOS"
       OR NOT _auval_version STREQUAL ""
       OR NOT _auval_executable_path STREQUAL ""
       OR NOT _auval_executable_sha256 STREQUAL ""
       OR NOT _auval_exit_code STREQUAL "not-run")
        message(FATAL_ERROR "non-macOS auval report is incoherent")
    endif()
endif()

file(SHA256 "${SYNTH_HOST_MATRIX_PATH}" _expected_host_sha256)
_synth_read_json(_host_definition "${SYNTH_HOST_MATRIX_PATH}" "required host definition")
string(JSON _definition_row_count LENGTH "${_host_definition}" rows)
string(JSON _definition_check_count LENGTH "${_host_definition}" checks)
string(JSON _host_sha256 GET "${_validation}" hosts definition_sha256)
string(JSON _host_row_count LENGTH "${_validation}" hosts rows)
if(NOT _host_sha256 STREQUAL _expected_host_sha256
   OR NOT _host_row_count EQUAL 11
   OR NOT _definition_row_count EQUAL 11
   OR NOT _definition_check_count EQUAL 9)
    message(FATAL_ERROR "host matrix definition/hash/row count is inconsistent")
endif()
list(FIND _build_paths "validation/required-host-matrix.json" _host_evidence_index)
if(_host_evidence_index EQUAL -1)
    message(FATAL_ERROR "hashed host matrix is absent from build evidence")
endif()
list(GET _build_hashes ${_host_evidence_index} _host_evidence_sha256)
if(NOT _host_evidence_sha256 STREQUAL _expected_host_sha256)
    message(FATAL_ERROR "build evidence host matrix differs from the hashed source definition")
endif()
math(EXPR _last_host_row "${_host_row_count} - 1")
foreach(_row_index RANGE 0 ${_last_host_row})
    string(JSON _row_status GET "${_validation}" hosts rows ${_row_index} status)
    string(JSON _row_kind GET "${_validation}" hosts rows ${_row_index} kind)
    string(JSON _row_name GET "${_validation}" hosts rows ${_row_index} name)
    string(JSON _row_version GET "${_validation}" hosts rows ${_row_index} required_version)
    string(JSON _row_platform GET "${_validation}" hosts rows ${_row_index} platform)
    string(JSON _row_source GET "${_validation}" hosts rows ${_row_index} source_url)
    string(JSON _expected_name GET "${_host_definition}" rows ${_row_index} name)
    string(JSON _expected_version GET "${_host_definition}" rows ${_row_index} required_version)
    string(JSON _expected_platform GET "${_host_definition}" rows ${_row_index} platform)
    string(JSON _expected_source GET "${_host_definition}" rows ${_row_index} source_url)
    string(JSON _expected_kind GET "${_host_definition}" rows ${_row_index} kind)
    if(_expected_version STREQUAL "\${PROJECT_VERSION}")
        set(_expected_version "${SYNTH_PROJECT_VERSION}")
    endif()
    if(NOT _row_name STREQUAL _expected_name
       OR NOT _row_version STREQUAL _expected_version
       OR NOT _row_platform STREQUAL _expected_platform
       OR NOT _row_source STREQUAL _expected_source
       OR NOT _row_kind STREQUAL _expected_kind)
        message(FATAL_ERROR "host row ${_row_index} does not match its hashed definition")
    endif()
    string(JSON _check_count LENGTH "${_validation}" hosts rows ${_row_index} checks)
    _synth_require_status("${_row_status}" "host row")
    if(NOT _check_count EQUAL 9)
        message(FATAL_ERROR "host row ${_row_index} does not contain all required checks")
    endif()
    if(_row_kind STREQUAL "commercial-host" AND _row_status STREQUAL "pass")
        message(FATAL_ERROR "unexecuted commercial host row cannot be pass")
    endif()
    math(EXPR _last_check "${_check_count} - 1")
    foreach(_check_index RANGE 0 ${_last_check})
        string(JSON _check_status GET "${_validation}" hosts rows ${_row_index} checks ${_check_index} status)
        string(JSON _check_name GET "${_validation}" hosts rows ${_row_index} checks ${_check_index} name)
        string(JSON _expected_check_name GET "${_host_definition}" checks ${_check_index})
        _synth_require_status("${_check_status}" "host check")
        if(NOT _check_name STREQUAL _expected_check_name)
            message(FATAL_ERROR "host row ${_row_index} check ${_check_index} does not match its definition")
        endif()
        if(_check_status STREQUAL "pass")
            message(FATAL_ERROR "unexecuted host check cannot be pass")
        endif()
    endforeach()
endforeach()

string(JSON _identity_approved GET "${_build_manifest}" identity identity_approved)
string(JSON _identity_manufacturer GET "${_build_manifest}" identity manufacturer_name)
string(JSON _identity_domain GET "${_build_manifest}" identity manufacturer_domain)
if(_identity_approved
   AND NOT _identity_manufacturer STREQUAL ""
   AND NOT _identity_manufacturer STREQUAL "yourcompany"
   AND NOT _identity_domain MATCHES "(^|\\.)yourcompany($|\\.)")
    set(_expected_identity_status "pass")
else()
    set(_expected_identity_status "blocked")
endif()
string(JSON _identity_status GET "${_validation}" aggregates identity status)
_synth_require_status("${_identity_status}" "identity aggregate")
if(NOT _identity_status STREQUAL _expected_identity_status)
    message(FATAL_ERROR "identity aggregate is inconsistent with the build manifest identity")
endif()
foreach(_aggregate IN ITEMS validation distribution release)
    string(JSON _aggregate_status GET "${_validation}" aggregates ${_aggregate} status)
    _synth_require_status("${_aggregate_status}" "${_aggregate} aggregate")
    if(NOT _aggregate_status STREQUAL "blocked")
        message(FATAL_ERROR "${_aggregate} aggregate must remain blocked under current gates")
    endif()
endforeach()
message(STATUS "Verified linked validation evidence; release remains blocked")
