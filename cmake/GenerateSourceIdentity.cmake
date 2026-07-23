cmake_minimum_required(VERSION 3.24)

foreach(_required IN ITEMS
        SYNTH_SOURCE_ROOT
        SYNTH_GIT_EXECUTABLE
        SYNTH_TEMPLATE
        SYNTH_OUTPUT)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} is required to generate source identity")
    endif()
endforeach()

execute_process(
    COMMAND "${SYNTH_GIT_EXECUTABLE}" -C "${SYNTH_SOURCE_ROOT}"
            rev-parse --verify HEAD
    RESULT_VARIABLE _commit_status
    OUTPUT_VARIABLE SYNTH_GENERATED_SOURCE_COMMIT
    ERROR_VARIABLE _commit_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ENCODING UTF-8)
execute_process(
    COMMAND "${SYNTH_GIT_EXECUTABLE}" -C "${SYNTH_SOURCE_ROOT}"
            rev-parse --verify "HEAD^{tree}"
    RESULT_VARIABLE _tree_status
    OUTPUT_VARIABLE SYNTH_GENERATED_SOURCE_TREE
    ERROR_VARIABLE _tree_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ENCODING UTF-8)
execute_process(
    COMMAND "${SYNTH_GIT_EXECUTABLE}" -C "${SYNTH_SOURCE_ROOT}"
            diff --no-ext-diff --binary HEAD --
    RESULT_VARIABLE _diff_status
    OUTPUT_VARIABLE _tracked_diff
    ERROR_VARIABLE _diff_error
    ENCODING UTF-8)
execute_process(
    COMMAND "${SYNTH_GIT_EXECUTABLE}" -C "${SYNTH_SOURCE_ROOT}"
            status --porcelain=v1 --untracked-files=normal --ignore-submodules=none
    RESULT_VARIABLE _status_status
    OUTPUT_VARIABLE _working_status
    ERROR_VARIABLE _status_error
    ENCODING UTF-8)

if(NOT _commit_status EQUAL 0 OR NOT _tree_status EQUAL 0
   OR NOT _diff_status EQUAL 0 OR NOT _status_status EQUAL 0)
    message(FATAL_ERROR
        "Reference source identity requires readable Git state:\n"
        "${_commit_error}${_tree_error}${_diff_error}${_status_error}")
endif()
string(LENGTH "${SYNTH_GENERATED_SOURCE_COMMIT}" _commit_length)
string(LENGTH "${SYNTH_GENERATED_SOURCE_TREE}" _tree_length)
if((NOT _commit_length EQUAL 40 AND NOT _commit_length EQUAL 64)
   OR (NOT _tree_length EQUAL 40 AND NOT _tree_length EQUAL 64)
   OR SYNTH_GENERATED_SOURCE_COMMIT MATCHES "[^0-9a-f]"
   OR SYNTH_GENERATED_SOURCE_TREE MATCHES "[^0-9a-f]")
    message(FATAL_ERROR "Reference source identity received malformed Git object IDs")
endif()

string(SHA256 SYNTH_GENERATED_SOURCE_CONTENT
       "${SYNTH_GENERATED_SOURCE_TREE}\n${_tracked_diff}")
if(_working_status STREQUAL "")
    set(SYNTH_GENERATED_SOURCE_DIRTY 0)
else()
    set(SYNTH_GENERATED_SOURCE_DIRTY 1)
endif()

set(SYNTH_GENERATED_GIT_EXECUTABLE "${SYNTH_GIT_EXECUTABLE}")
string(REPLACE "\\" "\\\\" SYNTH_GENERATED_GIT_EXECUTABLE
       "${SYNTH_GENERATED_GIT_EXECUTABLE}")
string(REPLACE "\"" "\\\"" SYNTH_GENERATED_GIT_EXECUTABLE
       "${SYNTH_GENERATED_GIT_EXECUTABLE}")

get_filename_component(_output_directory "${SYNTH_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_directory}")
configure_file("${SYNTH_TEMPLATE}" "${SYNTH_OUTPUT}" @ONLY)
