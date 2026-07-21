include_guard(GLOBAL)

set(_synth_reserved_identity_codes
    "0000"
    "DEMO"
    "NONE"
    "NULL"
    "TEST")

function(synth_collect_product_identity_errors output_variable)
    set(_identity_errors "")

    string(STRIP "${SYNTH_PRODUCT_NAME}" _product_name)
    string(TOLOWER "${_product_name}" _product_name_lower)
    if(_product_name STREQUAL ""
       OR _product_name_lower MATCHES
          "^(your[ _-]*product|product[ _-]*name|placeholder|untitled|minimoog)$")
        list(APPEND _identity_errors "product name is empty or placeholder")
    endif()

    string(STRIP "${SYNTH_MANUFACTURER_NAME}" _manufacturer_name)
    string(TOLOWER "${_manufacturer_name}" _manufacturer_name_lower)
    if(_manufacturer_name STREQUAL ""
       OR _manufacturer_name_lower MATCHES
          "^(your[ _-]*company|company[ _-]*name|manufacturer|placeholder|untitled)$")
        list(APPEND _identity_errors "manufacturer name is empty or placeholder")
    endif()

    string(STRIP "${SYNTH_MANUFACTURER_DOMAIN}" _manufacturer_domain)
    string(TOLOWER "${_manufacturer_domain}" _manufacturer_domain_lower)
    if(_manufacturer_domain STREQUAL ""
       OR NOT _manufacturer_domain MATCHES
          "^[A-Za-z0-9]+([.-][A-Za-z0-9]+)+$"
       OR _manufacturer_domain_lower MATCHES
          "(^|[.])yourcompany($|[.])|(^|[.])example[.](com|net|org)$")
        list(APPEND _identity_errors
            "reverse-DNS domain is empty, malformed, or placeholder")
    endif()

    string(STRIP "${SYNTH_BUNDLE_ID}" _bundle_id)
    string(TOLOWER "${_bundle_id}" _bundle_id_lower)
    set(_bundle_prefix "${_manufacturer_domain}.")
    string(FIND "${_bundle_id}" "${_bundle_prefix}" _bundle_prefix_position)
    if(_bundle_id STREQUAL ""
       OR NOT _bundle_id MATCHES "^[A-Za-z0-9]+([.-][A-Za-z0-9]+)+$"
       OR NOT _bundle_prefix_position EQUAL 0
       OR _bundle_id_lower MATCHES
          "(^|[.])(yourproduct|your-product|product|placeholder)($|[.])"
       OR _bundle_id_lower MATCHES "^com[.]example([.]|$)")
        list(APPEND _identity_errors
            "bundle identifier is empty, malformed, or placeholder")
    endif()

    foreach(_code_kind IN ITEMS manufacturer product)
        if(_code_kind STREQUAL "manufacturer")
            set(_code "${SYNTH_MANUFACTURER_CODE}")
        else()
            set(_code "${SYNTH_PRODUCT_CODE}")
        endif()
        string(LENGTH "${_code}" _code_length)
        if(NOT _code_length EQUAL 4 OR NOT _code MATCHES "^[A-Za-z0-9]+$")
            list(APPEND _identity_errors
                "${_code_kind} code must be exactly four ASCII alphanumeric characters")
        endif()

        string(TOUPPER "${_code}" _code_upper)
        if(_code_upper IN_LIST _synth_reserved_identity_codes)
            list(APPEND _identity_errors "${_code_kind} code is reserved")
        endif()
        set(_${_code_kind}_code_upper "${_code_upper}")
    endforeach()

    if(_manufacturer_code_upper STREQUAL _product_code_upper
       AND NOT _manufacturer_code_upper STREQUAL "")
        list(APPEND _identity_errors
            "manufacturer and product codes must be distinct")
    endif()

    if(NOT SYNTH_IDENTITY_APPROVED)
        list(APPEND _identity_errors "identity is not approved")
    endif()

    set(${output_variable} "${_identity_errors}" PARENT_SCOPE)
endfunction()
