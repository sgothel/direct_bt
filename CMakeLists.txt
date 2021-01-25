cmake_minimum_required (VERSION 3.13.0 FATAL_ERROR)
project (direct_bt LANGUAGES C CXX)

FIND_PACKAGE (Threads REQUIRED)

if(NOT SKIP_TINYB)
    FIND_PACKAGE (PkgConfig REQUIRED)
    PKG_CHECK_MODULES (GLIB2 REQUIRED glib-2.0>=2.40)
    PKG_CHECK_MODULES (GIO REQUIRED gio-2.0>=2.40)
    PKG_CHECK_MODULES (GIO-UNIX REQUIRED gio-unix-2.0>=2.40)
endif()

# set(CMAKE_MESSAGE_LOG_LEVEL DEBUG)

include(jaulib/JaulibSetup.cmake)
JaulibSetup()

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/version.c.in
                ${CMAKE_CURRENT_BINARY_DIR}/src/version.c)

# this is the library version, independant of git revision
set (direct_bt_VERSION_MAJOR ${VERSION_MAJOR})
set (direct_bt_VERSION_MINOR ${VERSION_MINOR})
set (direct_bt_VERSION_PATCH ${VERSION_PATCH})
set (direct_bt_VERSION_STRING "${direct_bt_VERSION_MAJOR}.${direct_bt_VERSION_MINOR}.${direct_bt_VERSION_PATCH}")

set (tinyb_VERSION_MAJOR ${VERSION_MAJOR})
set (tinyb_VERSION_MINOR ${VERSION_MINOR})
set (tinyb_VERSION_PATCH ${VERSION_PATCH})
set (tinyb_VERSION_STRING "${tinyb_VERSION_MAJOR}.${tinyb_VERSION_MINOR}.${tinyb_VERSION_PATCH}")

set (CMAKE_SWIG_FLAGS "")

set (direct_bt_DOCS_DIR documentation)

find_path (SYSTEM_USR_DIR "stdlib.h")
include_directories (${SYSTEM_USR_DIR})

add_subdirectory (jaulib)

add_subdirectory (src/direct_bt)

if(NOT SKIP_TINYB)
    add_subdirectory (src/tinyb)
endif()

option (BUILDJAVA "Build Java API." OFF)

IF(BUILDJAVA)
    configure_file (${CMAKE_CURRENT_SOURCE_DIR}/java/manifest.txt.in ${CMAKE_CURRENT_BINARY_DIR}/java/manifest.txt)
    add_subdirectory (java)
    add_subdirectory (examples/java)
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
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/api/tinyb/*.hpp ${CMAKE_CURRENT_SOURCE_DIR}/api/direct_bt/*.hpp ${CMAKE_CURRENT_SOURCE_DIR}/java/org/direct_bt/*.java ${CMAKE_CURRENT_SOURCE_DIR}/java/tinyb/dbus/*java ${CMAKE_CURRENT_SOURCE_DIR}/java/jau/direct_bt/*java
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
endif (BUILDEXAMPLES)

if (BUILD_TESTING)
  enable_testing ()
  add_subdirectory (test/ieee11073)
  add_subdirectory (test/direct_bt)
  if (BUILDJAVA)
      add_subdirectory (test/java)
  endif(BUILDJAVA)
endif(BUILD_TESTING)


