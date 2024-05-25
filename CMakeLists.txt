cmake_minimum_required (VERSION 3.21.0 FATAL_ERROR)
set(USE_LIBCURL OFF CACHE BOOL "" FORCE)
include(jaulib/JaulibSetup.cmake)
JaulibPreset()

if(JAU_CMAKE_HARD_PRESETS)
    set(BUILD_TRIAL ON CACHE STRING "" FORCE)
endif()

project (direct_bt LANGUAGES C CXX)

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)

# set(CMAKE_MESSAGE_LOG_LEVEL DEBUG)

if (BUILD_TRIAL)
    set (BUILD_TESTING ON)
endif(BUILD_TRIAL)

if(NOT DEFINED DONT_USE_RTTI)
    set(DONT_USE_RTTI ON)
    message(STATUS "${PROJECT_NAME} DONT_USE_RTTI ${DONT_USE_RTTI} (direct_bt default)")
else()
    message(STATUS "${PROJECT_NAME} DONT_USE_RTTI ${DONT_USE_RTTI} (user)")
endif()

JaulibSetup()

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.clangd)
# Fixup clangd w/ absolute path
set (direct_bt_clangd_source_args "-isystem, ${CMAKE_CURRENT_SOURCE_DIR}/jaulib/include, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/jaulib/src, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/jaulib/java_jni/jni, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/jaulib/java_jni/jni/jau")
set (direct_bt_clangd_source_args "${direct_bt_clangd_source_args}, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/jaulib/include/catch2_jau, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/jaulib/test")
set (direct_bt_clangd_source_args "${direct_bt_clangd_source_args} -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/api/include, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/src/direct_bt, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/src/direct_bt/ieee11073, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/java/jni, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/java/jni/direct_bt, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/test/direct_bt, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/test/ieee11073, -isystem, ${CMAKE_CURRENT_SOURCE_DIR}/trial/direct_bt")
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/clangd.in
                ${CMAKE_CURRENT_SOURCE_DIR}/.clangd)
endif()

# this is the library version, derived from git tag and revision
set (direct_bt_VERSION_MAJOR      ${VERSION_MAJOR})
set (direct_bt_VERSION_MINOR      ${VERSION_MINOR})
set (direct_bt_VERSION_PATCH      ${VERSION_PATCH})
set (direct_bt_VERSION_COMMITS    ${VERSION_COMMITS})
set (direct_bt_VERSION_SHA1       ${VERSION_SHA1})
set (direct_bt_VERSION_SHA1_SHORT ${VERSION_SHA1_SHORT})
set (direct_bt_GIT_DIRTY          ${GIT_WORKDIR_DIRTY})
set (direct_bt_VERSION_LONG       ${VERSION_LONG})
set (direct_bt_VERSION_SHORT      ${VERSION_SHORT})
set (direct_bt_VERSION_API        ${VERSION_API})

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/version.cpp.in
                ${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp)

set (CMAKE_SWIG_FLAGS "")

set (direct_bt_DOCS_DIR documentation)

find_path (SYSTEM_USR_DIR "stdlib.h")
include_directories (${SYSTEM_USR_DIR})

add_subdirectory (jaulib)

add_subdirectory (src/direct_bt)

option (BUILDJAVA "Build Java API." OFF)

IF(BUILDJAVA)
    configure_file (${CMAKE_CURRENT_SOURCE_DIR}/java/manifest.txt.in ${CMAKE_CURRENT_BINARY_DIR}/java/manifest.txt)
    configure_file (${CMAKE_CURRENT_SOURCE_DIR}/java_fat/manifest-fat.txt.in ${CMAKE_CURRENT_BINARY_DIR}/java_fat/manifest-fat.txt)
    add_subdirectory (java)
    add_subdirectory (java_fat)
ENDIF(BUILDJAVA)

# add a target to generate API documentation with Doxygen
find_package (Doxygen)
if (DOXYGEN_FOUND)
  configure_file (${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.cpp.out @ONLY)
  if (BUILDJAVA)
    configure_file (${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.java.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.java.out @ONLY)
    add_custom_command (OUTPUT ${direct_bt_DOCS_DIR}/cpp ${direct_bt_DOCS_DIR}/java
      COMMAND ${CMAKE_COMMAND} -E make_directory ${direct_bt_DOCS_DIR}
      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.cpp.out
      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.java.out
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/api/direct_bt/*.hpp ${CMAKE_CURRENT_SOURCE_DIR}/java/org/direct_bt/*.java ${CMAKE_CURRENT_SOURCE_DIR}/java/jau/direct_bt/*java
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
    add_custom_target(doc
      DEPENDS ${direct_bt_DOCS_DIR}/cpp ${direct_bt_DOCS_DIR}/java)
  else ()
    add_custom_command (OUTPUT ${direct_bt_DOCS_DIR}/cpp
      COMMAND ${CMAKE_COMMAND} -E make_directory ${direct_bt_DOCS_DIR}
      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.cpp.out
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/api/
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
    add_custom_target(doc
      DEPENDS ${direct_bt_DOCS_DIR}/cpp)
  endif ()
endif (DOXYGEN_FOUND)

if (BUILDEXAMPLES)
  add_subdirectory (examples)
  if (BUILDJAVA)
      add_subdirectory (examples/java)
  endif(BUILDJAVA)
endif (BUILDEXAMPLES)

if (BUILD_TESTING)
  enable_testing ()
  add_subdirectory (test/ieee11073)
  add_subdirectory (test/direct_bt)
  if (BUILDJAVA)
      configure_file (${CMAKE_CURRENT_SOURCE_DIR}/test/java/manifest.txt.in ${CMAKE_CURRENT_BINARY_DIR}/test/java/manifest.txt)
      add_subdirectory (test/java)
  endif(BUILDJAVA)
endif(BUILD_TESTING)

if (BUILD_TRIAL)
  enable_testing ()
  add_subdirectory (trial/direct_bt)
  if (BUILDJAVA)
      configure_file (${CMAKE_CURRENT_SOURCE_DIR}/trial/java/manifest.txt.in ${CMAKE_CURRENT_BINARY_DIR}/trial/java/manifest.txt)
      add_subdirectory (trial/java)
  endif(BUILDJAVA)
endif(BUILD_TRIAL)


