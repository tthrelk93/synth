include(FetchContent)

set(SYNTH_REQUIRED_JUCE_VERSION "8.0.10")
set(SYNTH_REQUIRED_JUCE_COMMIT "3af3ce009f6a02f6fa651008fffb5b41743a9fab")

function(_synth_read_git_revision checkout_path output_variable)
    execute_process(
        COMMAND git -C "${checkout_path}" rev-parse --show-toplevel
        RESULT_VARIABLE _git_root_status
        OUTPUT_VARIABLE _git_root
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    file(REAL_PATH "${checkout_path}" _checkout_real_path)
    if(_git_root_status OR NOT _git_root STREQUAL _checkout_real_path)
        set(${output_variable} "" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND git -C "${checkout_path}" rev-parse HEAD
        RESULT_VARIABLE _git_head_status
        OUTPUT_VARIABLE _git_head
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(_git_head_status)
        set(${output_variable} "" PARENT_SCOPE)
    else()
        set(${output_variable} "${_git_head}" PARENT_SCOPE)
    endif()
endfunction()

if(SYNTH_JUCE_SOURCE_DIR)
    _synth_read_git_revision("${SYNTH_JUCE_SOURCE_DIR}" _synth_override_revision)
    if(NOT _synth_override_revision)
        message(FATAL_ERROR
            "SYNTH_JUCE_SOURCE_DIR must name a JUCE Git checkout at commit ${SYNTH_REQUIRED_JUCE_COMMIT}.")
    endif()
    if(NOT _synth_override_revision STREQUAL SYNTH_REQUIRED_JUCE_COMMIT)
        message(FATAL_ERROR
            "SYNTH_JUCE_SOURCE_DIR revision mismatch: expected ${SYNTH_REQUIRED_JUCE_COMMIT} but found ${_synth_override_revision}.")
    endif()
    FetchContent_Declare(juce SOURCE_DIR "${SYNTH_JUCE_SOURCE_DIR}")
else()
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG "${SYNTH_REQUIRED_JUCE_COMMIT}"
        GIT_SHALLOW FALSE
        GIT_PROGRESS TRUE)
endif()

FetchContent_MakeAvailable(juce)
_synth_read_git_revision("${juce_SOURCE_DIR}" SYNTH_RESOLVED_JUCE_COMMIT)
if(NOT SYNTH_RESOLVED_JUCE_COMMIT STREQUAL SYNTH_REQUIRED_JUCE_COMMIT)
    message(FATAL_ERROR
        "Resolved JUCE revision mismatch: expected ${SYNTH_REQUIRED_JUCE_COMMIT} but found ${SYNTH_RESOLVED_JUCE_COMMIT}.")
endif()

set(SYNTH_RESOLVED_JUCE_COMMIT "${SYNTH_RESOLVED_JUCE_COMMIT}" CACHE INTERNAL
    "Verified JUCE revision used by this build")
message(STATUS
    "Resolved JUCE ${SYNTH_REQUIRED_JUCE_VERSION} at commit ${SYNTH_RESOLVED_JUCE_COMMIT}")
