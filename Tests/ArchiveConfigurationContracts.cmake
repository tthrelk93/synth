cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
        SYNTH_SOURCE_ROOT
        SYNTH_CONTRACT_ROOT
        SYNTH_JUCE_SOURCE_DIR
        SYNTH_CMAKE_GENERATOR)
    if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
        message(FATAL_ERROR
            "${_required_variable} is required for archive configuration contract tests")
    endif()
endforeach()

if(NOT DEFINED SYNTH_GIT_EXECUTABLE OR SYNTH_GIT_EXECUTABLE STREQUAL "")
    unset(SYNTH_GIT_EXECUTABLE)
    unset(SYNTH_GIT_EXECUTABLE CACHE)
    find_program(SYNTH_GIT_EXECUTABLE git REQUIRED)
endif()

file(REMOVE_RECURSE "${SYNTH_CONTRACT_ROOT}")
file(MAKE_DIRECTORY "${SYNTH_CONTRACT_ROOT}")
set(_archive_file "${SYNTH_CONTRACT_ROOT}/source.tar")
set(_archive_source "${SYNTH_CONTRACT_ROOT}/source")
file(MAKE_DIRECTORY "${_archive_source}")

execute_process(
    COMMAND "${SYNTH_GIT_EXECUTABLE}" -C "${SYNTH_SOURCE_ROOT}"
            archive --format=tar --output "${_archive_file}" HEAD
    RESULT_VARIABLE _archive_status
    OUTPUT_VARIABLE _archive_stdout
    ERROR_VARIABLE _archive_stderr
    ENCODING UTF-8)
if(NOT _archive_status EQUAL 0)
    message(FATAL_ERROR
        "Could not create repository archive:\n${_archive_stdout}\n${_archive_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xvf "${_archive_file}"
    WORKING_DIRECTORY "${_archive_source}"
    RESULT_VARIABLE _extract_status
    OUTPUT_QUIET
    ERROR_VARIABLE _extract_stderr
    ENCODING UTF-8)
if(NOT _extract_status EQUAL 0)
    message(FATAL_ERROR "Could not extract repository archive:\n${_extract_stderr}")
endif()

# Exercise the CMakeLists.txt under test before it has necessarily been committed.
# On an exact-head verification run this is byte-identical to the archived copy.
configure_file("${SYNTH_SOURCE_ROOT}/CMakeLists.txt"
               "${_archive_source}/CMakeLists.txt" COPYONLY)
if(EXISTS "${_archive_source}/.git")
    message(FATAL_ERROR "The extracted repository context unexpectedly contains .git")
endif()

set(_common_arguments
    -S "${_archive_source}"
    "-DSYNTH_JUCE_SOURCE_DIR=${SYNTH_JUCE_SOURCE_DIR}"
    -DSYNTH_BUILD_VALIDATORS=OFF
    -DSYNTH_VALIDATE_DISTRIBUTION_IDENTITY=ON
    -DCMAKE_BUILD_TYPE=Release
    -G "${SYNTH_CMAKE_GENERATOR}")
if(DEFINED SYNTH_CMAKE_GENERATOR_PLATFORM
   AND NOT SYNTH_CMAKE_GENERATOR_PLATFORM STREQUAL "")
    list(APPEND _common_arguments -A "${SYNTH_CMAKE_GENERATOR_PLATFORM}")
endif()
if(DEFINED SYNTH_CMAKE_GENERATOR_TOOLSET
   AND NOT SYNTH_CMAKE_GENERATOR_TOOLSET STREQUAL "")
    list(APPEND _common_arguments -T "${SYNTH_CMAKE_GENERATOR_TOOLSET}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${_common_arguments}
            -B "${SYNTH_CONTRACT_ROOT}/tests-off-build"
            -DSYNTH_BUILD_TESTS=OFF
    RESULT_VARIABLE _tests_off_status
    OUTPUT_VARIABLE _tests_off_stdout
    ERROR_VARIABLE _tests_off_stderr
    ENCODING UTF-8)
if(NOT _tests_off_status EQUAL 0)
    message(FATAL_ERROR
        "A tests-off distribution configure from a no-.git archive must pass:\n"
        "${_tests_off_stdout}\n${_tests_off_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${_common_arguments}
            -B "${SYNTH_CONTRACT_ROOT}/tests-on-build"
            -DSYNTH_BUILD_TESTS=ON
    RESULT_VARIABLE _tests_on_status
    OUTPUT_VARIABLE _tests_on_stdout
    ERROR_VARIABLE _tests_on_stderr
    ENCODING UTF-8)
set(_tests_on_output "${_tests_on_stdout}\n${_tests_on_stderr}")
if(_tests_on_status EQUAL 0)
    message(FATAL_ERROR
        "A tests-on configure from a no-.git archive unexpectedly passed")
endif()
if(NOT _tests_on_output MATCHES
   "Reference rendering requires git rev-parse HEAD")
    message(FATAL_ERROR
        "The tests-on archive failure did not report the stable Git diagnostic:\n"
        "${_tests_on_output}")
endif()

message(STATUS "Archive configuration contracts passed")
