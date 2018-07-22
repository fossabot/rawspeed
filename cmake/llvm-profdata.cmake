if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "Compiler is not clang! Aborting...")
endif()

if(NOT RAWSPEED_ENABLE_INSTR_PROFILE)
  message(SEND_ERROR "RAWSPEED_ENABLE_INSTR_PROFILE is not set.")
endif()

if(NOT (RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "COVERAGE" OR
        RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "PGOGEN"))
  message(SEND_ERROR "RAWSPEED_ENABLE_INSTR_PROFILE has unknown value: \"${RAWSPEED_ENABLE_INSTR_PROFILE}\".")
endif()

find_package(LLVMProfData REQUIRED)
find_package(Find REQUIRED)

add_custom_target(
  profdata
  COMMAND "${FIND_PATH}" "${PROJECT_BINARY_DIR}" -type f -name '*.profraw' -exec "${LLVMPROFDATA_PATH}" merge -o "${RAWSPEED_PROFDATA_FILE}" {} + > /dev/null
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  COMMENT "Running llvm-profdata tool on all the *.profraw files"
)

add_custom_target(
  profdata-clean
  COMMAND "${FIND_PATH}" "${PROJECT_BINARY_DIR}" -type f -name '*.profdata' -delete > /dev/null
  COMMAND "${FIND_PATH}" "${PROJECT_BINARY_DIR}" -type f -name '*.profraw'  -delete > /dev/null
  COMMAND "${CMAKE_COMMAND}" -E remove -f "${RAWSPEED_PROFDATA_FILE}" > /dev/null
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  COMMENT "Removing all the *.profdata and *.profraw files"
)

if(RAWSPEED_ENABLE_SAMPLE_BASED_TESTING)
  add_custom_target(
    instrumentation-profile
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target profdata-clean
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target rstest-check
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target profdata
    DEPENDS rstest
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Doing everything to generate clean fresh *.profdata file"
    USES_TERMINAL
  )
endif()
