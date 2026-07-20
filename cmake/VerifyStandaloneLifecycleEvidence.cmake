cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_BUILD_ROOT
        SYNTH_STAGE_DIRECTORY
        SYNTH_STANDALONE_REPORT_PATH
        SYNTH_REPEAT_COUNT
        SYNTH_SYSTEM_NAME
        SYNTH_ARCHITECTURE
        SYNTH_CONFIGURATION
        SYNTH_PROJECT_VERSION)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${_required_variable} is required to verify standalone lifecycle evidence")
    endif()
endforeach()
if(NOT SYNTH_REPEAT_COUNT MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "standalone lifecycle repeat count must be a positive integer")
endif()

function(_synth_read_object output_variable path description)
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

function(_synth_verify_file root relative_path expected_sha256 description)
    _synth_existing_file_within(_path "${root}" "${relative_path}" "${description}")
    file(SHA256 "${_path}" _actual_sha256)
    if(NOT _actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR "${description} hash mismatch: ${relative_path}")
    endif()
endfunction()

function(_synth_png_dimensions path output_width output_height)
    file(SIZE "${path}" _size)
    if(_size LESS 24)
        message(FATAL_ERROR "standalone screenshot is empty or truncated: ${path}")
    endif()
    file(READ "${path}" _header HEX LIMIT 24)
    string(TOLOWER "${_header}" _header)
    string(SUBSTRING "${_header}" 0 32 _signature_and_ihdr)
    if(NOT _signature_and_ihdr STREQUAL "89504e470d0a1a0a0000000d49484452")
        message(FATAL_ERROR "standalone screenshot is not a PNG with an IHDR header: ${path}")
    endif()
    string(SUBSTRING "${_header}" 32 8 _width_hex)
    string(SUBSTRING "${_header}" 40 8 _height_hex)
    math(EXPR _width "0x${_width_hex}")
    math(EXPR _height "0x${_height_hex}")
    if(_width LESS 1 OR _height LESS 1)
        message(FATAL_ERROR "standalone screenshot dimensions are empty: ${path}")
    endif()
    set(${output_width} "${_width}" PARENT_SCOPE)
    set(${output_height} "${_height}" PARENT_SCOPE)
endfunction()

function(_synth_require_true json assertion_name context)
    string(JSON _value ERROR_VARIABLE _error GET "${json}" assertions "${assertion_name}")
    if(_error OR NOT _value)
        message(FATAL_ERROR "${context} failed required window assertion: ${assertion_name}")
    endif()
endfunction()

cmake_path(ABSOLUTE_PATH SYNTH_BUILD_ROOT NORMALIZE OUTPUT_VARIABLE _build_root_lexical)
file(REAL_PATH "${SYNTH_STAGE_DIRECTORY}" _stage_root)
cmake_path(ABSOLUTE_PATH SYNTH_STANDALONE_REPORT_PATH NORMALIZE
           OUTPUT_VARIABLE _aggregate_path)
set(_required_aggregate_path "${_build_root_lexical}/validation/standalone-lifecycle-report.json")
if(NOT _aggregate_path STREQUAL _required_aggregate_path)
    message(FATAL_ERROR "standalone aggregate must use its fixed build evidence path")
endif()
file(REAL_PATH "${SYNTH_BUILD_ROOT}" _build_root)
set(_aggregate_path "${_build_root}/validation/standalone-lifecycle-report.json")

_synth_read_object(_build_manifest "${_stage_root}/build-manifest.json" "build manifest")
_synth_read_object(_aggregate "${_aggregate_path}" "standalone lifecycle aggregate")

string(JSON _schema GET "${_aggregate}" schema_version)
string(JSON _tool_version GET "${_aggregate}" tool_version)
string(JSON _aggregate_status GET "${_aggregate}" status)
string(JSON _aggregate_repeat_count GET "${_aggregate}" repeat_count)
string(JSON _aggregate_os GET "${_aggregate}" environment os)
string(JSON _aggregate_arch GET "${_aggregate}" environment architecture)
string(JSON _aggregate_configuration GET "${_aggregate}" environment configuration)
string(JSON _virtual_display_provider GET "${_aggregate}" environment virtual_display_provider)
if(SYNTH_SYSTEM_NAME STREQUAL "Linux")
    if(NOT _virtual_display_provider MATCHES "^(inherited-x11|xvfb-run)$")
        message(FATAL_ERROR "standalone lifecycle has an invalid Linux virtual-display provider")
    endif()
elseif(NOT _virtual_display_provider STREQUAL "native")
    message(FATAL_ERROR "standalone lifecycle has an invalid native display provider")
endif()
if(NOT _schema STREQUAL "1"
   OR NOT _tool_version STREQUAL SYNTH_PROJECT_VERSION
   OR NOT _aggregate_status STREQUAL "pass"
   OR NOT _aggregate_repeat_count STREQUAL SYNTH_REPEAT_COUNT
   OR NOT _aggregate_os STREQUAL SYNTH_SYSTEM_NAME
   OR NOT _aggregate_arch STREQUAL SYNTH_ARCHITECTURE
   OR NOT _aggregate_configuration STREQUAL SYNTH_CONFIGURATION)
    message(FATAL_ERROR "standalone lifecycle aggregate header/status is inconsistent")
endif()

string(JSON _product_count LENGTH "${_build_manifest}" products)
set(_standalone_count 0)
set(_expected_executable_relative "")
if(_product_count GREATER 0)
    math(EXPR _last_product "${_product_count} - 1")
    foreach(_product_index RANGE 0 ${_last_product})
        string(JSON _format GET "${_build_manifest}" products ${_product_index} format)
        if(NOT _format STREQUAL "Standalone")
            continue()
        endif()
        math(EXPR _standalone_count "${_standalone_count} + 1")
        string(JSON _expected_product_relative GET "${_build_manifest}" products ${_product_index} relative_path)
        string(JSON _payload_root_relative GET "${_build_manifest}" products ${_product_index} payload_root_relative_path)
        string(JSON _expected_product_sha256 GET "${_build_manifest}" products ${_product_index} aggregate_sha256)
        _synth_normalize_relative(_expected_product_relative "${_expected_product_relative}" "Standalone product")
        _synth_normalize_relative(_payload_root_relative "${_payload_root_relative}" "Standalone payload root")
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
            set(_stage_file_relative "${_payload_root_relative}/${_file_relative}")
            _synth_verify_file("${_stage_root}" "${_stage_file_relative}" "${_file_sha256}" "Standalone payload")
            string(APPEND _aggregate_input "${_file_sha256}  ${_file_relative}\n")
            if(SYNTH_SYSTEM_NAME STREQUAL "Darwin")
                if(_file_relative MATCHES "^Contents/MacOS/[^/]+$")
                    list(APPEND _executable_candidates "${_stage_file_relative}|${_file_sha256}")
                endif()
            elseif(_stage_file_relative STREQUAL _expected_product_relative)
                list(APPEND _executable_candidates "${_stage_file_relative}|${_file_sha256}")
            endif()
        endforeach()
        string(SHA256 _actual_product_sha256 "${_aggregate_input}")
        if(NOT _actual_product_sha256 STREQUAL _expected_product_sha256)
            message(FATAL_ERROR "Standalone product aggregate hash mismatch")
        endif()
        list(LENGTH _executable_candidates _executable_count)
        if(NOT _executable_count EQUAL 1)
            message(FATAL_ERROR "Standalone product must declare exactly one executable payload")
        endif()
        list(GET _executable_candidates 0 _executable_entry)
        string(REPLACE "|" ";" _executable_parts "${_executable_entry}")
        list(GET _executable_parts 0 _expected_executable_relative)
        list(GET _executable_parts 1 _expected_executable_sha256)
    endforeach()
endif()
if(NOT _standalone_count EQUAL 1)
    message(FATAL_ERROR "build manifest must declare exactly one Standalone product")
endif()

string(JSON _declared_product_relative GET "${_aggregate}" executable product_relative_path)
string(JSON _declared_product_sha256 GET "${_aggregate}" executable product_aggregate_sha256)
string(JSON _declared_executable_relative GET "${_aggregate}" executable relative_path)
string(JSON _declared_executable_sha256 GET "${_aggregate}" executable sha256)
if(NOT _declared_product_relative STREQUAL _expected_product_relative
   OR NOT _declared_product_sha256 STREQUAL _expected_product_sha256
   OR NOT _declared_executable_relative STREQUAL _expected_executable_relative
   OR NOT _declared_executable_sha256 STREQUAL _expected_executable_sha256)
    message(FATAL_ERROR "standalone executable identity/hash differs from the staged build manifest")
endif()
_synth_verify_file("${_stage_root}" "${_declared_executable_relative}"
                   "${_declared_executable_sha256}" "Standalone executable")

file(RELATIVE_PATH _stage_from_build "${_build_root}" "${_stage_root}")
string(REPLACE "\\" "/" _stage_from_build "${_stage_from_build}")
_synth_normalize_relative(_stage_from_build "${_stage_from_build}" "Stage directory")

set(_expected_modes normal invalid no-device)
list(LENGTH _expected_modes _mode_count)
math(EXPR _expected_run_count "${_mode_count} * ${SYNTH_REPEAT_COUNT}")
string(JSON _actual_run_count LENGTH "${_aggregate}" runs)
if(NOT _actual_run_count EQUAL _expected_run_count)
    message(FATAL_ERROR "standalone aggregate does not contain exactly three modes times the repeat count")
endif()

set(_run_index 0)
foreach(_mode IN LISTS _expected_modes)
    string(REPLACE "-" "_" _mode_key "${_mode}")
    foreach(_repeat RANGE 1 ${SYNTH_REPEAT_COUNT})
        string(JSON _run GET "${_aggregate}" runs ${_run_index})
        string(JSON _run_mode GET "${_run}" mode)
        string(JSON _run_repeat GET "${_run}" repeat)
        string(JSON _run_status GET "${_run}" status)
        string(JSON _exit_code GET "${_run}" exit_code)
        if(NOT _run_mode STREQUAL _mode
           OR NOT _run_repeat STREQUAL _repeat
           OR NOT _run_status STREQUAL "pass"
           OR NOT _exit_code STREQUAL "0")
            message(FATAL_ERROR "standalone run ${_run_index} mode/repeat/status is inconsistent")
        endif()

        set(_run_root "validation/standalone/${_mode}/repeat-${_repeat}")
        set(_report_relative "${_run_root}/report.json")
        set(_screenshot_relative "${_run_root}/screenshot.png")
        set(_log_relative "${_run_root}/process.log")
        foreach(_kind IN ITEMS report screenshot process_log)
            string(JSON _declared_path GET "${_run}" ${_kind}_path)
            string(JSON _declared_sha256 GET "${_run}" ${_kind}_sha256)
            if(_kind STREQUAL "report")
                set(_expected_path "${_report_relative}")
            elseif(_kind STREQUAL "screenshot")
                set(_expected_path "${_screenshot_relative}")
            else()
                set(_expected_path "${_log_relative}")
            endif()
            if(NOT _declared_path STREQUAL _expected_path)
                message(FATAL_ERROR "standalone run ${_run_index} has an unexpected ${_kind} path")
            endif()
            _synth_verify_file("${_build_root}" "${_declared_path}" "${_declared_sha256}"
                               "Standalone ${_kind} evidence")
            set(_baseline_variable "_${_mode_key}_${_kind}_sha256")
            if(_repeat EQUAL 1)
                set(${_baseline_variable} "${_declared_sha256}")
            elseif(NOT _declared_sha256 STREQUAL "${${_baseline_variable}}")
                message(FATAL_ERROR
                    "standalone ${_mode} ${_kind} evidence is not byte-identical across repeats")
            endif()
        endforeach()

        set(_isolation_root "validation/standalone/runtime-isolation/${_mode}")
        set(_settings_relative "${_isolation_root}/settings/MiniMoog.settings")
        set(_preset_relative "${_isolation_root}/presets")
        set(_expected_command cmake -E env
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
        if(_virtual_display_provider STREQUAL "inherited-x11")
            list(APPEND _expected_command "DISPLAY=<INHERITED_X11_DISPLAY>")
        elseif(_virtual_display_provider STREQUAL "xvfb-run")
            list(APPEND _expected_command xvfb-run -a)
        endif()
        list(APPEND _expected_command
            "${_stage_from_build}/${_expected_executable_relative}"
            --synth-lifecycle-test "${_mode}"
            --report "${_report_relative}"
            --screenshot "${_screenshot_relative}"
            --settings "${_settings_relative}"
            --preset-dir "${_preset_relative}")
        set(_actual_command "")
        string(JSON _command_count LENGTH "${_run}" command)
        if(_command_count GREATER 0)
            math(EXPR _last_token "${_command_count} - 1")
            foreach(_token_index RANGE 0 ${_last_token})
                string(JSON _token GET "${_run}" command ${_token_index})
                list(APPEND _actual_command "${_token}")
            endforeach()
        endif()
        if(NOT _actual_command STREQUAL _expected_command)
            message(FATAL_ERROR "standalone run ${_run_index} exact normalized command is inconsistent")
        endif()

        _synth_read_object(_app_report "${_build_root}/${_report_relative}"
                           "standalone application report")
        string(JSON _app_schema GET "${_app_report}" schema_version)
        string(JSON _app_version GET "${_app_report}" tool_version)
        string(JSON _app_mode GET "${_app_report}" mode)
        string(JSON _app_status GET "${_app_report}" status)
        string(JSON _window_visible GET "${_app_report}" sequence window_visible)
        string(JSON _configuration_start GET "${_app_report}" sequence device_configuration_start)
        string(JSON _first_discovery GET "${_app_report}" sequence first_audio_device_discovery)
        string(JSON _failure_count LENGTH "${_app_report}" failures)
        if(NOT _app_schema STREQUAL "1"
           OR NOT _app_version STREQUAL SYNTH_PROJECT_VERSION
           OR NOT _app_mode STREQUAL _mode
           OR NOT _app_status STREQUAL "pass"
           OR NOT _failure_count EQUAL 0
           OR _window_visible LESS 1
           OR NOT _configuration_start GREATER _window_visible)
            message(FATAL_ERROR "standalone application report ${_run_index} header/order/status is inconsistent")
        endif()
        foreach(_assertion IN ITEMS
                top_level_visible
                top_level_showing
                native_peer_present
                window_bounds_nonempty
                visible_before_device_configuration
                device_discovery_after_visibility
                custom_editor_visible
                custom_editor_showing
                custom_editor_bounds_nonempty
                editor_constrainer_present
                editor_constrainer_limits_valid
                window_decorator_constrainer_active
                editor_resize_propagated_to_window
                editor_resize_round_trip
                status_label_showing
                status_label_bounds_nonempty
                piano_key_initially_released
                piano_note_round_trip
                piano_key_released
                status_nonempty
                status_matches_device_state
                invalid_error_nonempty
                required_current_device_absent
                no_device_midi_inputs_disabled
                no_device_discovery_skipped
                settings_path_matches_request
                preset_directory_matches_request
                screenshot_render_warmup_valid
                png_valid_nonempty
                png_dimensions_match)
            _synth_require_true("${_app_report}" "${_assertion}" "Standalone run ${_run_index}")
        endforeach()
        string(JSON _piano_key_count GET "${_app_report}" assertions visible_piano_key_count)
        if(_piano_key_count LESS 1)
            message(FATAL_ERROR "standalone run ${_run_index} found no visible PianoKey components")
        endif()

        foreach(_resize_field IN ITEMS
                minimum_editor_width minimum_editor_height
                maximum_editor_width maximum_editor_height
                original_editor_width original_editor_height
                requested_editor_width requested_editor_height
                resized_editor_width resized_editor_height
                resized_content_width resized_content_height
                original_window_width original_window_height
                resized_window_width resized_window_height
                restored_editor_width restored_editor_height
                restored_content_width restored_content_height
                restored_window_width restored_window_height)
            string(JSON _resize_${_resize_field} GET "${_app_report}" resize ${_resize_field})
        endforeach()
        if(_resize_minimum_editor_width LESS 1
           OR _resize_minimum_editor_height LESS 1
           OR _resize_maximum_editor_width LESS _resize_minimum_editor_width
           OR _resize_maximum_editor_height LESS _resize_minimum_editor_height
           OR _resize_original_editor_width LESS _resize_minimum_editor_width
           OR _resize_original_editor_width GREATER _resize_maximum_editor_width
           OR _resize_original_editor_height LESS _resize_minimum_editor_height
           OR _resize_original_editor_height GREATER _resize_maximum_editor_height
           OR (_resize_requested_editor_width EQUAL _resize_original_editor_width
               AND _resize_requested_editor_height EQUAL _resize_original_editor_height)
           OR _resize_requested_editor_width LESS _resize_minimum_editor_width
           OR _resize_requested_editor_width GREATER _resize_maximum_editor_width
           OR _resize_requested_editor_height LESS _resize_minimum_editor_height
           OR _resize_requested_editor_height GREATER _resize_maximum_editor_height
           OR NOT _resize_resized_editor_width EQUAL _resize_requested_editor_width
           OR NOT _resize_resized_editor_height EQUAL _resize_requested_editor_height
           OR NOT _resize_resized_content_width EQUAL _resize_requested_editor_width
           OR NOT _resize_resized_content_height EQUAL _resize_requested_editor_height)
            message(FATAL_ERROR
                "standalone run ${_run_index} editor constrainer/resize evidence is inconsistent")
        endif()
        math(EXPR _expected_resized_window_width
             "${_resize_original_window_width} + ${_resize_requested_editor_width} - ${_resize_original_editor_width}")
        math(EXPR _expected_resized_window_height
             "${_resize_original_window_height} + ${_resize_requested_editor_height} - ${_resize_original_editor_height}")
        if(NOT _resize_resized_window_width EQUAL _expected_resized_window_width
           OR NOT _resize_resized_window_height EQUAL _expected_resized_window_height
           OR NOT _resize_restored_editor_width EQUAL _resize_original_editor_width
           OR NOT _resize_restored_editor_height EQUAL _resize_original_editor_height
           OR NOT _resize_restored_content_width EQUAL _resize_original_editor_width
           OR NOT _resize_restored_content_height EQUAL _resize_original_editor_height
           OR NOT _resize_restored_window_width EQUAL _resize_original_window_width
           OR NOT _resize_restored_window_height EQUAL _resize_original_window_height)
            message(FATAL_ERROR
                "standalone run ${_run_index} editor-driven resize round-trip is inconsistent")
        endif()

        string(JSON _current_device GET "${_app_report}" device current_device)
        string(JSON _open_error GET "${_app_report}" device open_error)
        string(JSON _midi_count GET "${_app_report}" device enabled_midi_input_count)
        string(JSON _status_text GET "${_app_report}" device status_text)
        string(JSON _discovery_count GET "${_app_report}" discovery audio_device_discovery_count)
        string(JSON _midi_enumerated GET "${_app_report}" discovery midi_enumeration_performed)
        string(JSON _callbacks_wired GET "${_app_report}" discovery callbacks_wired)
        if(_status_text STREQUAL "" OR NOT _status_text MATCHES "MIDI inputs enabled: ${_midi_count}$")
            message(FATAL_ERROR "standalone run ${_run_index} status text does not bind its MIDI state")
        endif()
        if(_mode STREQUAL "invalid" AND (_open_error STREQUAL "" OR NOT _current_device STREQUAL ""))
            message(FATAL_ERROR "standalone invalid mode did not retain the required unavailable state")
        endif()
        if(_mode STREQUAL "no-device")
            if(NOT _current_device STREQUAL ""
               OR NOT _midi_count EQUAL 0
               OR NOT _discovery_count EQUAL 0
               OR NOT _first_discovery EQUAL 0
               OR _midi_enumerated
               OR _callbacks_wired)
                message(FATAL_ERROR
                    "standalone no-device mode performed forbidden audio/MIDI discovery")
            endif()
        elseif(_discovery_count LESS 1
               OR NOT _first_discovery GREATER _configuration_start
               OR NOT _midi_enumerated
               OR NOT _callbacks_wired)
            message(FATAL_ERROR
                "standalone ${_mode} mode did not defer discovery until after visibility")
        endif()

        string(JSON _settings_path GET "${_app_report}" settings path)
        string(JSON _settings_match GET "${_app_report}" settings matches_requested_path)
        string(JSON _preset_path GET "${_app_report}" presets path)
        string(JSON _preset_match GET "${_app_report}" presets matches_requested_path)
        if(NOT _settings_path STREQUAL _settings_relative
           OR NOT _preset_path STREQUAL _preset_relative
           OR NOT _settings_match OR NOT _preset_match)
            message(FATAL_ERROR
                "standalone run ${_run_index} did not retain its isolated settings/preset paths")
        endif()

        _synth_existing_file_within(_screenshot "${_build_root}" "${_screenshot_relative}"
                                    "Standalone screenshot")
        _synth_png_dimensions("${_screenshot}" _png_width _png_height)
        string(JSON _window_width GET "${_app_report}" window width)
        string(JSON _window_height GET "${_app_report}" window height)
        if(NOT _png_width EQUAL _window_width OR NOT _png_height EQUAL _window_height)
            message(FATAL_ERROR "standalone screenshot dimensions differ from the app report")
        endif()

        math(EXPR _run_index "${_run_index} + 1")
    endforeach()
endforeach()

if(EXISTS "${_build_root}/validation/standalone/runtime-isolation"
   OR IS_SYMLINK "${_build_root}/validation/standalone/runtime-isolation")
    message(FATAL_ERROR "standalone runtime-isolation directory remains after validation")
endif()

message(STATUS "Verified ${_expected_run_count} standalone lifecycle launches and evidence files")
