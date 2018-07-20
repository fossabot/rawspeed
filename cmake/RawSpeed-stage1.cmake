# Enable ExternalProject CMake module
include(ExternalProject)

message(STATUS "Configuring Stage-1 of RawSpeed")

if(NOT RAWSPEED_ENABLE_SAMPLE_BASED_TESTING)
  message(SEND_ERROR "Warning: Can not generate instrumentation profile without"
            " sample-based testing. Please pass "
            "-DENABLE_SAMPLEBASED_TESTING:BOOL=ON and pass correct "
            "-DRAWSPEED_REFERENCE_SAMPLE_ARCHIVE:STRING=\"<path to "
            "https://raw.pixls.us/data-unique/ checkout>\"")
  return()
endif()

set(RAWSPEED_STAGE1_BINARY_DIR "${PROJECT_BINARY_DIR}/rawspeed-stage1/build"
    CACHE PATH "Where the Stage-1 of RawSpeed is being built" FORCE)

ExternalProject_Add(
  rawspeed-stage1
  PREFIX            "${PROJECT_BINARY_DIR}/rawspeed-stage1"
  SOURCE_DIR        "${PROJECT_SOURCE_DIR}"
  BINARY_DIR        "${RAWSPEED_STAGE1_BINARY_DIR}"
  CMAKE_CACHE_ARGS
    -DBINARY_PACKAGE_BUILD:BOOL=OFF
    -DBUILD_BENCHMARKING:BOOL=OFF
    -DBUILD_DOCS:BOOL=OFF
    -DBUILD_FUZZERS:BOOL=OFF
    -DBUILD_TESTING:BOOL=OFF
    -DBUILD_TOOLS:BOOL=ON
    -DCMAKE_BUILD_TYPE:STRING=Release
    -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
    -DRAWSPEED_BUILD_STAGE1:BOOL=OFF
    -DRAWSPEED_ENABLE_DEBUG_INFO:BOOL=OFF
    -DRAWSPEED_ENABLE_INSTR_PROFILE:STRING=PGOGEN
    -DRAWSPEED_ENABLE_SAMPLE_BASED_TESTING:BOOL=ON
    -DRAWSPEED_PROFDATA_FILE:PATH=${RAWSPEED_PROFDATA_FILE}
    -DRAWSPEED_REFERENCE_SAMPLE_ARCHIVE:PATH=${RAWSPEED_REFERENCE_SAMPLE_ARCHIVE}
    -DUSE_CLANG_TIDY:BOOL=OFF
    -DUSE_IWYU:BOOL=OFF
    -DUSE_LLVM_OPT_REPORT:BOOL=OFF
  BUILD_ALWAYS      TRUE # Always build it.
  EXCLUDE_FROM_ALL  TRUE # But do not want to build it implicitly
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)

ExternalProject_Add_Step(rawspeed-stage1 clean
  COMMAND           "${CMAKE_COMMAND}" --build "${RAWSPEED_STAGE1_BINARY_DIR}" --target clean
  COMMENT           "Running 'clean' target within Stage-1 of RawSpeed"
  ALWAYS            FALSE
  EXCLUDE_FROM_MAIN TRUE
)
ExternalProject_Add_StepTargets(rawspeed-stage1 clean)

ExternalProject_Add_Step(rawspeed-stage1 instrumentation-profile
  COMMAND           "${CMAKE_COMMAND}" --build "${RAWSPEED_STAGE1_BINARY_DIR}" --target instrumentation-profile
  COMMENT           "Gene fresh instrumentation profile within Stage-1 of RawSpeed"
  BYPRODUCTS        "${RAWSPEED_PROFDATA_FILE}"
  ALWAYS            TRUE
  EXCLUDE_FROM_MAIN TRUE
)
ExternalProject_Add_StepTargets(rawspeed-stage1 instrumentation-profile)
add_dependencies(rawspeed-stage1-instrumentation-profile rawspeed-stage1)

message(STATUS "Done configuring Stage-1 of RawSpeed")
