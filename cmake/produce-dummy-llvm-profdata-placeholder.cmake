if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "Compiler is not clang! Aborting...")
endif()

if(EXISTS "${RAWSPEED_PROFDATA_FILE}")
  # If the .profdata already exists, let's assume that it can be used,
  # and not override it with a placeholder.
  return()
endif()

include(CheckCXXSourceRuns)

find_package(LLVMProfData REQUIRED)

set(PLACEHOLDER_PROFRAW "${CMAKE_CURRENT_BINARY_DIR}/placeholder.profraw")

set(CMAKE_REQUIRED_FLAGS_ORIG "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate=\"${PLACEHOLDER_PROFRAW}\"")

CHECK_CXX_SOURCE_RUNS("int main(int argc, char* argv[]) {
  return 0;
}" RAWSPEED_PRODUCE_PLACEHOLDER_PROFRAW)

if(NOT RAWSPEED_PRODUCE_PLACEHOLDER_PROFRAW)
  message(SEND_ERROR "Failed to compile a simple testcase to produce dummy clang *.profraw profile")
  return()
endif()

set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS_ORIG}")

execute_process(
  COMMAND "${LLVMPROFDATA_PATH}" merge -o "${RAWSPEED_PROFDATA_FILE}" "${PLACEHOLDER_PROFRAW}"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  RESULT_VARIABLE RAWSPEED_PRODUCE_PLACEHOLDER_PROFDATA
)

if(NOT RAWSPEED_PRODUCE_PLACEHOLDER_PROFDATA EQUAL 0)
  message(SEND_ERROR "Failed to merge dummy *.profraw into dummy *.profdata profile")
  return()
endif()
