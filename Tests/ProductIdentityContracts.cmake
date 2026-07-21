cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS SYNTH_SOURCE_ROOT SYNTH_CONTRACT_ROOT)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR
            "${_required_variable} is required for product identity contract tests")
    endif()
endforeach()

include("${SYNTH_SOURCE_ROOT}/cmake/ValidateProductIdentity.cmake")
include("${SYNTH_SOURCE_ROOT}/cmake/ProductIdentity.cmake")

synth_collect_product_identity_errors(_approved_identity_errors)
if(_approved_identity_errors)
    list(JOIN _approved_identity_errors "; " _approved_identity_error_text)
    message(FATAL_ERROR
        "The approved product identity is invalid: ${_approved_identity_error_text}")
endif()

file(REMOVE_RECURSE "${SYNTH_CONTRACT_ROOT}")
file(MAKE_DIRECTORY "${SYNTH_CONTRACT_ROOT}")

function(_synth_run_invalid_identity_case case_name expected_error product_name
         manufacturer_name manufacturer_domain bundle_id manufacturer_code
         product_code identity_approved)
    set(_identity_file "${SYNTH_CONTRACT_ROOT}/${case_name}-identity.cmake")
    set(_build_directory "${SYNTH_CONTRACT_ROOT}/${case_name}-build")
    file(WRITE "${_identity_file}"
        "set(SYNTH_PRODUCT_NAME \"${product_name}\")\n"
        "set(SYNTH_MANUFACTURER_NAME \"${manufacturer_name}\")\n"
        "set(SYNTH_MANUFACTURER_WEBSITE \"https://github.com/tthrelk93\")\n"
        "set(SYNTH_MANUFACTURER_EMAIL \"\")\n"
        "set(SYNTH_MANUFACTURER_DOMAIN \"${manufacturer_domain}\")\n"
        "set(SYNTH_MANUFACTURER_CODE \"${manufacturer_code}\")\n"
        "set(SYNTH_PRODUCT_CODE \"${product_code}\")\n"
        "set(SYNTH_BUNDLE_ID \"${bundle_id}\")\n"
        "set(SYNTH_IDENTITY_APPROVED ${identity_approved})\n")

    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -S "${SYNTH_SOURCE_ROOT}"
                -B "${_build_directory}"
                "-DSYNTH_PRODUCT_IDENTITY_FILE=${_identity_file}"
                -DSYNTH_VALIDATE_DISTRIBUTION_IDENTITY=ON
                -DSYNTH_BUILD_TESTS=OFF
                -DSYNTH_BUILD_VALIDATORS=OFF
        RESULT_VARIABLE _configure_status
        OUTPUT_VARIABLE _configure_stdout
        ERROR_VARIABLE _configure_stderr
        ENCODING UTF-8)
    set(_configure_output "${_configure_stdout}\n${_configure_stderr}")
    if(_configure_status EQUAL 0)
        message(FATAL_ERROR
            "Invalid identity case ${case_name} unexpectedly configured successfully")
    endif()
    if(NOT _configure_output MATCHES
       "Distribution identity validation failed \\(BLD-007\\)")
        message(FATAL_ERROR
            "Invalid identity case ${case_name} did not fail at the BLD-007 guard:\n${_configure_output}")
    endif()
    string(REGEX REPLACE "[ \t\r\n]+" " " _normalized_configure_output
                         "${_configure_output}")
    string(FIND "${_normalized_configure_output}" "${expected_error}"
           _expected_error_position)
    if(_expected_error_position LESS 0)
        message(FATAL_ERROR
            "Invalid identity case ${case_name} did not report '${expected_error}':\n${_configure_output}")
    endif()
endfunction()

set(_valid_product "TTH Model One")
set(_valid_manufacturer "TTH Audio")
set(_valid_domain "io.github.tthrelk93")
set(_valid_bundle "io.github.tthrelk93.TTHModelOne")
set(_valid_manufacturer_code "TTHA")
set(_valid_product_code "TM01")

_synth_run_invalid_identity_case(
    empty-product "product name is empty or placeholder"
    "" "${_valid_manufacturer}" "${_valid_domain}" "${_valid_bundle}"
    "${_valid_manufacturer_code}" "${_valid_product_code}" ON)
_synth_run_invalid_identity_case(
    placeholder-product "product name is empty or placeholder"
    "Your Product" "${_valid_manufacturer}" "${_valid_domain}" "${_valid_bundle}"
    "${_valid_manufacturer_code}" "${_valid_product_code}" ON)
_synth_run_invalid_identity_case(
    placeholder-bundle "bundle identifier is empty, malformed, or placeholder"
    "${_valid_product}" "${_valid_manufacturer}" "${_valid_domain}"
    "com.example.product" "${_valid_manufacturer_code}" "${_valid_product_code}" ON)
_synth_run_invalid_identity_case(
    empty-bundle "bundle identifier is empty, malformed, or placeholder"
    "${_valid_product}" "${_valid_manufacturer}" "${_valid_domain}" ""
    "${_valid_manufacturer_code}" "${_valid_product_code}" ON)
_synth_run_invalid_identity_case(
    malformed-domain "reverse-DNS domain is empty, malformed, or placeholder"
    "${_valid_product}" "${_valid_manufacturer}" "not a domain" "${_valid_bundle}"
    "${_valid_manufacturer_code}" "${_valid_product_code}" ON)
_synth_run_invalid_identity_case(
    short-code "manufacturer code must be exactly four ASCII alphanumeric characters"
    "${_valid_product}" "${_valid_manufacturer}" "${_valid_domain}" "${_valid_bundle}"
    "ABC" "${_valid_product_code}" ON)
_synth_run_invalid_identity_case(
    non-printable-code "product code must be exactly four ASCII alphanumeric characters"
    "${_valid_product}" "${_valid_manufacturer}" "${_valid_domain}" "${_valid_bundle}"
    "${_valid_manufacturer_code}" "A B1" ON)
_synth_run_invalid_identity_case(
    duplicate-codes "manufacturer and product codes must be distinct"
    "${_valid_product}" "${_valid_manufacturer}" "${_valid_domain}" "${_valid_bundle}"
    "TTHA" "TTHA" ON)
_synth_run_invalid_identity_case(
    reserved-code "manufacturer code is reserved"
    "${_valid_product}" "${_valid_manufacturer}" "${_valid_domain}" "${_valid_bundle}"
    "TEST" "${_valid_product_code}" ON)
_synth_run_invalid_identity_case(
    unapproved "identity is not approved"
    "${_valid_product}" "${_valid_manufacturer}" "${_valid_domain}" "${_valid_bundle}"
    "${_valid_manufacturer_code}" "${_valid_product_code}" OFF)

message(STATUS "Product identity validation contracts passed")
