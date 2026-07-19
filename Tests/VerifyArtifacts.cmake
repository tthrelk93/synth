set(_model_d_artifact_variables VST3_ARTIFACT STANDALONE_ARTIFACT)
if(DEFINED AU_ARTIFACT)
    list(APPEND _model_d_artifact_variables AU_ARTIFACT)
endif()

foreach(_model_d_artifact_variable IN LISTS _model_d_artifact_variables)
    set(_model_d_artifact_path "${${_model_d_artifact_variable}}")
    if(_model_d_artifact_path STREQUAL "" OR NOT EXISTS "${_model_d_artifact_path}")
        message(FATAL_ERROR
            "Model D format artifact is missing: ${_model_d_artifact_variable}=${_model_d_artifact_path}")
    endif()
    message(STATUS "Verified ${_model_d_artifact_variable}: ${_model_d_artifact_path}")
endforeach()
