include_directories(
  ${PROJECT_SOURCE_DIR}/jaulib/include
  ${PROJECT_SOURCE_DIR}/api
)

# These examples use the standard separate compilation
# file(GLOB_RECURSE SOURCES_IDIOMATIC_EXAMPLES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "test_*.cpp")
set( SOURCES_IDIOMATIC_EXAMPLES
    test_client_server00.cpp
    test_client_server01_NoEnc.cpp
)

string( REPLACE ".cpp" "" BASENAMES_IDIOMATIC_EXAMPLES "${SOURCES_IDIOMATIC_EXAMPLES}" )
set( TARGETS_IDIOMATIC_EXAMPLES ${BASENAMES_IDIOMATIC_EXAMPLES} )

foreach( name ${TARGETS_IDIOMATIC_EXAMPLES} )
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} direct_bt catch2)
    add_dependencies(${name} direct_bt catch2)
    add_test (NAME ${name} COMMAND ${name})
endforeach()

