if(NOT RAWSPEED_ENABLE_INSTR_PROFILE)
  return()
endif()

string(TOUPPER "${RAWSPEED_ENABLE_INSTR_PROFILE}" RAWSPEED_ENABLE_INSTR_PROFILE)

if(NOT (RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "COVERAGE" OR
        RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "PGOGEN" OR
        RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "PGOUSE"))
  message(SEND_ERROR "Warning: RAWSPEED_ENABLE_INSTR_PROFILE has unknown value:"
         " ${RAWSPEED_ENABLE_INSTR_PROFILE}. "
         "Supported are: OFF; COVERAGE; PGOGEN, PGOUSE.")
  return()
endif()

if(RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "PGOGEN" AND
   NOT RAWSPEED_ENABLE_SAMPLE_BASED_TESTING)
  message(SEND_ERROR "Warning: Can not generate instrumentation profile without"
            " sample-based testing. Please pass "
            "-DENABLE_SAMPLEBASED_TESTING:BOOL=ON and pass correct "
            "-DRAWSPEED_REFERENCE_SAMPLE_ARCHIVE:STRING=\"<path to "
            "https://raw.pixls.us/data-unique/ checkout>\"")
  return()
endif()

if(RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "COVERAGE" OR
   RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "PGOGEN")
  add_feature_info("Generation of instrumentation-based profile" ON
                   "can be used for coverage report, or PGO.")
endif()

set(CFLAGS "")
set(LDFLAGS "")

if(RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "PGOUSE")
  add_feature_info("Use profile-guided optimization profile" ON "the existing "
                   "instrumentation-based profile will be used by the compiler.")
  set(RAWSPEED_BUILD_STAGE1 ON CACHE BOOL "" FORCE)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    include(produce-dummy-llvm-profdata-placeholder)
    set(CFLAGS "${CFLAGS} -fprofile-instr-use=\"${RAWSPEED_PROFDATA_FILE}\"")
  else()
    message(SEND_ERROR "UNIMPLEMENTED")
  endif()
endif()

if(RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "PGOGEN" AND
   CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CFLAGS "${CFLAGS} -fprofile-instr-generate=default-%m-%p.profraw")
endif()

if(RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "COVERAGE")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CFLAGS "${CFLAGS} -fcoverage-mapping")
  elseif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(CFLAGS "${CFLAGS} -fprofile-arcs -ftest-coverage")
    set(LDFLAGS "${LDFLAGS} --coverage")
  endif()
elseif(RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "PGOGEN")
  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(CFLAGS "${CFLAGS} -fprofile-generate")
  endif()
endif()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CFLAGS}")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CFLAGS} ${LDFLAGS}")
SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${CFLAGS} ${LDFLAGS}")
SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${CFLAGS} ${LDFLAGS}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
    (RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "COVERAGE" OR
     RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "PGOGEN"))
  include(llvm-profdata)
endif()

if(BUILD_TESTING AND RAWSPEED_ENABLE_INSTR_PROFILE STREQUAL "COVERAGE")
  add_feature_info("Code Coverage reporting" ON
                   "used for visualizing the test coverage of the source code")

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    include(llvm-cov)
  elseif(CMAKE_COMPILER_IS_GNUCXX OR
         CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    include(gcc-gcov)

    find_package(LCov)
    find_package(GenHtml)

    if(LCov_FOUND AND GenHtml_FOUND)
      include(lcov)
      include(genhtml)
      include(gcc-coverage)
    else()
      message(WARNING "Did not find lcov and genhtml. "
                      "Will not be able to generate HTML reports")
    endif()
  endif()
endif()
