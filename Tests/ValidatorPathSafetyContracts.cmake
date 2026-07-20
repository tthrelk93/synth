cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_BUILD_ROOT
        SYNTH_CONTRACT_ROOT
        SYNTH_SOURCE_ROOT
        SYNTH_SYSTEM_NAME)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required for validator path-safety contracts")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH SYNTH_BUILD_ROOT NORMALIZE OUTPUT_VARIABLE _build_root)
cmake_path(ABSOLUTE_PATH SYNTH_CONTRACT_ROOT NORMALIZE OUTPUT_VARIABLE _contract_root)
cmake_path(IS_PREFIX _build_root "${_contract_root}" NORMALIZE _contract_is_build_owned)
if(NOT _contract_is_build_owned OR _contract_root STREQUAL _build_root)
    message(FATAL_ERROR "validator path-safety fixture root must be nested under the build root")
endif()
if(IS_SYMLINK "${_contract_root}")
    message(FATAL_ERROR "refusing to clean a symlinked validator path-safety fixture root")
endif()
if(EXISTS "${_contract_root}")
    file(REMOVE_RECURSE "${_contract_root}")
endif()
file(MAKE_DIRECTORY "${_contract_root}")

file(READ "${SYNTH_SOURCE_ROOT}/CMakeLists.txt" _top_level_cmake_source)
file(READ "${SYNTH_SOURCE_ROOT}/cmake/StageDevelopmentArtifacts.cmake"
     _stage_development_source)
file(READ "${SYNTH_SOURCE_ROOT}/cmake/GenerateValidationManifest.cmake"
     _generate_validation_source)
file(READ "${SYNTH_SOURCE_ROOT}/cmake/VerifyValidationManifest.cmake"
     _verify_validation_source)

# Multi-value -D arguments are not portable through Visual Studio when they
# are packed into one quoted semicolon list. Require indexed arguments for the
# final report inventory and expected-evidence inventory instead.
foreach(_indexed_transport_contract IN ITEMS
        "SYNTH_TEST_REPORT_COUNT"
        "SYNTH_TEST_REPORT_"
        "SYNTH_EXPECTED_EVIDENCE_COUNT"
        "SYNTH_EXPECTED_EVIDENCE_")
    string(FIND "${_top_level_cmake_source}"
           "${_indexed_transport_contract}" _top_level_contract_position)
    if(_top_level_contract_position LESS 0)
        message(FATAL_ERROR
            "validation evidence does not use indexed cross-generator argument transport: ${_indexed_transport_contract}")
    endif()
endforeach()
foreach(_indexed_report_contract IN ITEMS
        "SYNTH_TEST_REPORT_COUNT"
        "SYNTH_TEST_REPORT_")
    string(FIND "${_stage_development_source}"
           "${_indexed_report_contract}" _stage_contract_position)
    if(_stage_contract_position LESS 0)
        message(FATAL_ERROR
            "development staging cannot reconstruct indexed report arguments: ${_indexed_report_contract}")
    endif()
endforeach()
foreach(_validation_source IN ITEMS
        _generate_validation_source
        _verify_validation_source)
    foreach(_indexed_evidence_contract IN ITEMS
            "SYNTH_EXPECTED_EVIDENCE_COUNT"
            "SYNTH_EXPECTED_EVIDENCE_")
        string(FIND "${${_validation_source}}"
               "${_indexed_evidence_contract}" _evidence_contract_position)
        if(_evidence_contract_position LESS 0)
            message(FATAL_ERROR
                "validation manifest script cannot reconstruct indexed evidence arguments: ${_indexed_evidence_contract}")
        endif()
    endforeach()
endforeach()
string(FIND "${_top_level_cmake_source}"
       "_model_d_validation_reports_argument"
       _packed_report_argument_position)
if(NOT _packed_report_argument_position LESS 0)
    message(FATAL_ERROR
        "final validation staging still transports reports as a packed semicolon argument")
endif()

file(READ "${SYNTH_SOURCE_ROOT}/cmake/ProvisionPluginval.cmake"
     _pluginval_provision_source)
foreach(_empty_tool_root_contract IN ITEMS
        "file(GLOB _unmarked_tool_entries"
        "if(_unmarked_tool_entries)"
        "unmarked pluginval tool root is not empty")
    string(FIND "${_pluginval_provision_source}"
           "${_empty_tool_root_contract}" _empty_tool_root_position)
    if(_empty_tool_root_position LESS 0)
        message(FATAL_ERROR
            "pluginval provisioning cannot safely adopt a generator-created empty tool root: ${_empty_tool_root_contract}")
    endif()
endforeach()

if(SYNTH_SYSTEM_NAME STREQUAL "Darwin")
    set(_asset "pluginval_macOS.zip")
    set(_archive_sha256 "3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f")
    set(_executable_relative "pluginval.app/Contents/MacOS/pluginval")
elseif(SYNTH_SYSTEM_NAME STREQUAL "Windows")
    set(_asset "pluginval_Windows.zip")
    set(_archive_sha256 "c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab")
    set(_executable_relative "pluginval.exe")
elseif(SYNTH_SYSTEM_NAME STREQUAL "Linux")
    set(_asset "pluginval_Linux.zip")
    set(_archive_sha256 "c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352")
    set(_executable_relative "pluginval")
else()
    message(FATAL_ERROR "unsupported validator path-safety platform: ${SYNTH_SYSTEM_NAME}")
endif()
set(_download_url
    "https://github.com/Tracktion/pluginval/releases/download/v1.0.4/${_asset}")

# Visual Studio may create the parent directory of a custom-command OUTPUT
# before the command runs. Provisioning may adopt that exact empty directory,
# but it must still reject and preserve any unmarked contents.
set(_empty_tool_build "${_contract_root}/empty-tool-root/build")
set(_empty_tool_output
    "${_empty_tool_build}/validation-tools/pluginval/v1.0.4")
file(MAKE_DIRECTORY "${_empty_tool_output}" "${_empty_tool_build}/validation")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSYNTH_BUILD_ROOT=${_empty_tool_build}"
        "-DSYNTH_PLUGINVAL_OUTPUT_DIRECTORY=${_empty_tool_output}"
        "-DSYNTH_PLUGINVAL_METADATA_PATH=${_empty_tool_build}/validation/pluginval-provision.json"
        "-DSYNTH_PLUGINVAL_STAMP_PATH=${_empty_tool_output}/provision.stamp"
        "-DSYNTH_PLUGINVAL_PLATFORM_ASSET=${_asset}"
        "-DSYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256=${_archive_sha256}"
        "-DSYNTH_PLUGINVAL_DOWNLOAD_URL=${_download_url}"
        "-DSYNTH_PLUGINVAL_EXECUTABLE_RELATIVE_PATH=${_executable_relative}"
        "-DSYNTH_PLUGINVAL_EXECUTABLE_OVERRIDE=${_empty_tool_build}/missing-pluginval"
        "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
        -P "${SYNTH_SOURCE_ROOT}/cmake/ProvisionPluginval.cmake"
    RESULT_VARIABLE _empty_tool_status
    OUTPUT_VARIABLE _empty_tool_stdout
    ERROR_VARIABLE _empty_tool_stderr
    ENCODING UTF-8)
set(_empty_tool_output_text "${_empty_tool_stdout}${_empty_tool_stderr}")
set(_empty_tool_marker "${_empty_tool_output}/.synth-pluginval-tool-root")
if(_empty_tool_status STREQUAL "0"
   OR NOT _empty_tool_output_text MATCHES "SYNTH_PLUGINVAL_EXECUTABLE does not exist"
   OR NOT EXISTS "${_empty_tool_marker}")
    message(FATAL_ERROR
        "pluginval did not safely adopt a generator-created empty tool root:\n${_empty_tool_output_text}")
endif()
file(READ "${_empty_tool_marker}" _empty_tool_marker_contents)
if(NOT _empty_tool_marker_contents STREQUAL "model-d-pluginval-tool-root-v1\n")
    message(FATAL_ERROR "adopted pluginval tool root has an invalid marker")
endif()

set(_nonempty_tool_build "${_contract_root}/nonempty-tool-root/build")
set(_nonempty_tool_output
    "${_nonempty_tool_build}/validation-tools/pluginval/v1.0.4")
set(_unowned_tool_file "${_nonempty_tool_output}/.unowned")
file(MAKE_DIRECTORY "${_nonempty_tool_output}" "${_nonempty_tool_build}/validation")
file(WRITE "${_unowned_tool_file}" "must-not-be-removed\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSYNTH_BUILD_ROOT=${_nonempty_tool_build}"
        "-DSYNTH_PLUGINVAL_OUTPUT_DIRECTORY=${_nonempty_tool_output}"
        "-DSYNTH_PLUGINVAL_METADATA_PATH=${_nonempty_tool_build}/validation/pluginval-provision.json"
        "-DSYNTH_PLUGINVAL_STAMP_PATH=${_nonempty_tool_output}/provision.stamp"
        "-DSYNTH_PLUGINVAL_PLATFORM_ASSET=${_asset}"
        "-DSYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256=${_archive_sha256}"
        "-DSYNTH_PLUGINVAL_DOWNLOAD_URL=${_download_url}"
        "-DSYNTH_PLUGINVAL_EXECUTABLE_RELATIVE_PATH=${_executable_relative}"
        "-DSYNTH_PLUGINVAL_EXECUTABLE_OVERRIDE=${_nonempty_tool_build}/missing-pluginval"
        "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
        -P "${SYNTH_SOURCE_ROOT}/cmake/ProvisionPluginval.cmake"
    RESULT_VARIABLE _nonempty_tool_status
    OUTPUT_VARIABLE _nonempty_tool_stdout
    ERROR_VARIABLE _nonempty_tool_stderr
    ENCODING UTF-8)
if(_nonempty_tool_status STREQUAL "0"
   OR NOT "${_nonempty_tool_stdout}${_nonempty_tool_stderr}" MATCHES
          "unmarked pluginval tool root is not empty"
   OR NOT EXISTS "${_unowned_tool_file}")
    message(FATAL_ERROR
        "pluginval did not preserve a nonempty unmarked tool root:\n${_nonempty_tool_stdout}${_nonempty_tool_stderr}")
endif()

function(_synth_require_rejected_without_external_mutation
         description status output external_directory)
    if("${status}" STREQUAL "0")
        message(FATAL_ERROR "${description} unexpectedly succeeded")
    endif()
    if(NOT "${output}" MATCHES "symlink ancestor")
        message(FATAL_ERROR
            "${description} failed for the wrong reason; expected symlink-ancestor rejection:\n${output}")
    endif()
    file(GLOB _external_entries LIST_DIRECTORIES true "${external_directory}/*")
    if(_external_entries)
        message(FATAL_ERROR
            "${description} mutated outside its declared build root: ${_external_entries}")
    endif()
endfunction()

# Provisioning must reject a symlinked validation-tools/pluginval parent before
# it creates the version leaf or ownership marker at the external target.
set(_provision_root "${_contract_root}/provision")
set(_provision_build "${_provision_root}/build")
set(_provision_external "${_provision_root}/external/pluginval")
file(MAKE_DIRECTORY "${_provision_build}/validation-tools" "${_provision_external}")
file(REAL_PATH "${_provision_build}" _provision_build_real)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
            "${_provision_external}"
            "${_provision_build}/validation-tools/pluginval"
    RESULT_VARIABLE _symlink_status
    ERROR_VARIABLE _symlink_error)
if(NOT _symlink_status STREQUAL "0")
    file(REMOVE_RECURSE "${_contract_root}")
    message(STATUS
        "Validator ancestor-symlink contracts skipped because symlink creation is unavailable: ${_symlink_error}")
    return()
endif()
set(_provision_output
    "${_provision_build_real}/validation-tools/pluginval/v1.0.4")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSYNTH_BUILD_ROOT=${_provision_build_real}"
        "-DSYNTH_PLUGINVAL_OUTPUT_DIRECTORY=${_provision_output}"
        "-DSYNTH_PLUGINVAL_METADATA_PATH=${_provision_build_real}/validation/pluginval-provision.json"
        "-DSYNTH_PLUGINVAL_STAMP_PATH=${_provision_output}/provision.stamp"
        "-DSYNTH_PLUGINVAL_PLATFORM_ASSET=${_asset}"
        "-DSYNTH_PLUGINVAL_EXPECTED_ARCHIVE_SHA256=${_archive_sha256}"
        "-DSYNTH_PLUGINVAL_DOWNLOAD_URL=${_download_url}"
        "-DSYNTH_PLUGINVAL_EXECUTABLE_RELATIVE_PATH=${_executable_relative}"
        "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
        -P "${SYNTH_SOURCE_ROOT}/cmake/ProvisionPluginval.cmake"
    RESULT_VARIABLE _provision_status
    OUTPUT_VARIABLE _provision_stdout
    ERROR_VARIABLE _provision_stderr
    ENCODING UTF-8)
_synth_require_rejected_without_external_mutation(
    "pluginval provisioning through a symlinked parent"
    "${_provision_status}" "${_provision_stdout}${_provision_stderr}"
    "${_provision_external}")
if(EXISTS "${_provision_build}/validation/pluginval-provision.json")
    message(FATAL_ERROR "rejected pluginval provisioning wrote metadata")
endif()

# Validation must reject a symlinked validation-ownership parent before it
# creates the ownership marker, evidence root, or top-level report.
set(_run_root "${_contract_root}/run")
set(_run_build "${_run_root}/build")
set(_run_external "${_run_root}/external/validation-ownership")
file(MAKE_DIRECTORY "${_run_build}/validation" "${_run_external}")
file(REAL_PATH "${_run_build}" _run_build_real)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
            "${_run_external}" "${_run_build}/validation-ownership"
    RESULT_VARIABLE _run_symlink_status
    ERROR_VARIABLE _run_symlink_error)
if(NOT _run_symlink_status STREQUAL "0")
    file(REMOVE_RECURSE "${_contract_root}")
    message(FATAL_ERROR
        "provision symlink creation succeeded but ownership symlink creation failed: ${_run_symlink_error}")
endif()
set(_run_report "${_run_build_real}/validation/pluginval-run-report.json")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSYNTH_BUILD_ROOT=${_run_build_real}"
        "-DSYNTH_STAGE_DIRECTORY=${_run_build_real}/stage"
        "-DSYNTH_PLUGINVAL_EXECUTABLE=${_run_build_real}/missing-pluginval"
        "-DSYNTH_PLUGINVAL_METADATA_PATH=${_run_build_real}/missing-provision.json"
        "-DSYNTH_PLUGINVAL_REPORT_PATH=${_run_report}"
        "-DSYNTH_VALIDATION_DIRECTORY=${_run_build_real}/validation"
        "-DSYNTH_REPEAT_COUNT=1"
        "-DSYNTH_SEED=0x4d6f64656c44"
        "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
        "-DSYNTH_ARCHITECTURE=contract-architecture"
        "-DSYNTH_CONFIGURATION=Contract"
        -P "${SYNTH_SOURCE_ROOT}/cmake/RunPluginval.cmake"
    RESULT_VARIABLE _run_status
    OUTPUT_VARIABLE _run_stdout
    ERROR_VARIABLE _run_stderr
    ENCODING UTF-8)
_synth_require_rejected_without_external_mutation(
    "pluginval validation through a symlinked ownership parent"
    "${_run_status}" "${_run_stdout}${_run_stderr}" "${_run_external}")
if(EXISTS "${_run_report}" OR EXISTS "${_run_build}/validation/pluginval")
    message(FATAL_ERROR "rejected pluginval validation created build evidence")
endif()

# A caller may spell an otherwise valid build root through a symlink (for
# example /tmp -> /private/tmp on macOS). Lexical ancestry checks must retain
# that spelling instead of comparing canonical candidates to a lexical root.
set(_alias_root "${_contract_root}/build-root-alias")
set(_alias_real_build "${_alias_root}/real-build")
set(_alias_build "${_alias_root}/build-link")
file(MAKE_DIRECTORY
    "${_alias_real_build}/validation"
    "${_alias_real_build}/validation-ownership")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
            "${_alias_real_build}" "${_alias_build}"
    RESULT_VARIABLE _alias_symlink_status
    ERROR_VARIABLE _alias_symlink_error)
if(NOT _alias_symlink_status STREQUAL "0")
    file(REMOVE_RECURSE "${_contract_root}")
    message(FATAL_ERROR
        "parent symlink creation succeeded but build-root alias creation failed: ${_alias_symlink_error}")
endif()
set(_alias_report "${_alias_build}/validation/pluginval-run-report.json")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSYNTH_BUILD_ROOT=${_alias_build}"
        "-DSYNTH_STAGE_DIRECTORY=${_alias_build}/stage"
        "-DSYNTH_PLUGINVAL_EXECUTABLE=${_alias_build}/missing-pluginval"
        "-DSYNTH_PLUGINVAL_METADATA_PATH=${_alias_build}/missing-provision.json"
        "-DSYNTH_PLUGINVAL_REPORT_PATH=${_alias_report}"
        "-DSYNTH_VALIDATION_DIRECTORY=${_alias_build}/validation"
        "-DSYNTH_REPEAT_COUNT=1"
        "-DSYNTH_SEED=0x4d6f64656c44"
        "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
        "-DSYNTH_ARCHITECTURE=contract-architecture"
        "-DSYNTH_CONFIGURATION=Contract"
        -P "${SYNTH_SOURCE_ROOT}/cmake/RunPluginval.cmake"
    RESULT_VARIABLE _alias_status
    OUTPUT_VARIABLE _alias_stdout
    ERROR_VARIABLE _alias_stderr
    ENCODING UTF-8)
if(_alias_status STREQUAL "0"
   OR NOT "${_alias_stdout}${_alias_stderr}" MATCHES
          "pluginval executable is missing")
    message(FATAL_ERROR
        "pluginval rejected a valid symlink-spelled build root before tool checks:\n${_alias_stdout}${_alias_stderr}")
endif()
if(EXISTS "${_alias_report}"
   OR EXISTS "${_alias_build}/validation/pluginval")
    message(FATAL_ERROR "build-root alias compatibility check created evidence")
endif()

# Standalone lifecycle evidence uses a separate marker in the same ownership
# directory. Its runner must apply the identical no-write-through-symlink rule.
set(_standalone_root "${_contract_root}/standalone")
set(_standalone_build "${_standalone_root}/build")
set(_standalone_stage "${_standalone_build}/stage")
set(_standalone_external
    "${_standalone_root}/external/validation-ownership")
file(MAKE_DIRECTORY
    "${_standalone_build}/validation"
    "${_standalone_stage}"
    "${_standalone_external}")
file(REAL_PATH "${_standalone_build}" _standalone_build_real)
file(REAL_PATH "${_standalone_stage}" _standalone_stage_real)
if(SYNTH_SYSTEM_NAME STREQUAL "Darwin")
    set(_standalone_product_relative "Standalone/MiniMoog.app")
    set(_standalone_payload_root "Standalone/MiniMoog.app")
    set(_standalone_file_relative "Contents/MacOS/MiniMoog")
elseif(SYNTH_SYSTEM_NAME STREQUAL "Windows")
    set(_standalone_product_relative "Standalone/MiniMoog.exe")
    set(_standalone_payload_root "Standalone")
    set(_standalone_file_relative "MiniMoog.exe")
else()
    set(_standalone_product_relative "Standalone/MiniMoog")
    set(_standalone_payload_root "Standalone")
    set(_standalone_file_relative "MiniMoog")
endif()
set(_standalone_executable
    "${_standalone_stage_real}/${_standalone_payload_root}/${_standalone_file_relative}")
get_filename_component(_standalone_executable_directory
                       "${_standalone_executable}" DIRECTORY)
file(MAKE_DIRECTORY "${_standalone_executable_directory}")
file(WRITE "${_standalone_executable}"
     "validator path-safety standalone fixture\n")
file(SHA256 "${_standalone_executable}" _standalone_executable_sha256)
string(SHA256 _standalone_aggregate_sha256
       "${_standalone_executable_sha256}  ${_standalone_file_relative}\n")
file(WRITE "${_standalone_stage_real}/build-manifest.json"
    "{\n"
    "  \"products\": [\n"
    "    {\n"
    "      \"format\": \"Standalone\",\n"
    "      \"relative_path\": \"${_standalone_product_relative}\",\n"
    "      \"payload_root_relative_path\": \"${_standalone_payload_root}\",\n"
    "      \"aggregate_sha256\": \"${_standalone_aggregate_sha256}\",\n"
    "      \"files\": [{\"path\": \"${_standalone_file_relative}\", \"sha256\": \"${_standalone_executable_sha256}\"}]\n"
    "    }\n"
    "  ]\n"
    "}\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
            "${_standalone_external}"
            "${_standalone_build_real}/validation-ownership"
    RESULT_VARIABLE _standalone_symlink_status
    ERROR_VARIABLE _standalone_symlink_error)
if(NOT _standalone_symlink_status STREQUAL "0")
    file(REMOVE_RECURSE "${_contract_root}")
    message(FATAL_ERROR
        "pluginval symlink creation succeeded but standalone ownership symlink creation failed: ${_standalone_symlink_error}")
endif()
set(_standalone_report
    "${_standalone_build_real}/validation/standalone-lifecycle-report.json")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSYNTH_BUILD_ROOT=${_standalone_build_real}"
        "-DSYNTH_STAGE_DIRECTORY=${_standalone_stage_real}"
        "-DSYNTH_STANDALONE_REPORT_PATH=${_standalone_report}"
        "-DSYNTH_VALIDATION_DIRECTORY=${_standalone_build_real}/validation"
        "-DSYNTH_REPEAT_COUNT=1"
        "-DSYNTH_SYSTEM_NAME=${SYNTH_SYSTEM_NAME}"
        "-DSYNTH_ARCHITECTURE=contract-architecture"
        "-DSYNTH_CONFIGURATION=Contract"
        "-DSYNTH_PROJECT_VERSION=1.0.0"
        "-DSYNTH_SOURCE_ROOT=${SYNTH_SOURCE_ROOT}"
        -P "${SYNTH_SOURCE_ROOT}/cmake/RunStandaloneLifecycle.cmake"
    RESULT_VARIABLE _standalone_status
    OUTPUT_VARIABLE _standalone_stdout
    ERROR_VARIABLE _standalone_stderr
    ENCODING UTF-8)
_synth_require_rejected_without_external_mutation(
    "standalone validation through a symlinked ownership parent"
    "${_standalone_status}" "${_standalone_stdout}${_standalone_stderr}"
    "${_standalone_external}")
if(EXISTS "${_standalone_report}"
   OR EXISTS "${_standalone_build_real}/validation/standalone")
    message(FATAL_ERROR "rejected standalone validation created build evidence")
endif()

file(REMOVE_RECURSE "${_contract_root}")
message(STATUS "Validator ancestor-symlink path-safety contracts passed")
