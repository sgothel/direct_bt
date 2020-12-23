include_directories(
  ${PROJECT_SOURCE_DIR}/jaulib/include
  ${PROJECT_SOURCE_DIR}/api
)

# These examples use the standard separate compilation
set( SOURCES_IDIOMATIC_EXAMPLES
    test_btaddress01.cpp
    test_uuid.cpp
    test_attpdu01.cpp
)

string( REPLACE ".cpp" "" BASENAMES_IDIOMATIC_EXAMPLES "${SOURCES_IDIOMATIC_EXAMPLES}" )
set( TARGETS_IDIOMATIC_EXAMPLES ${BASENAMES_IDIOMATIC_EXAMPLES} )

foreach( name ${TARGETS_IDIOMATIC_EXAMPLES} )
    add_executable(${name} ${name}.cpp)
endforeach()

set(ALL_EXAMPLE_TARGETS
  ${TARGETS_IDIOMATIC_EXAMPLES}
)

foreach(name ${ALL_EXAMPLE_TARGETS})
    target_link_libraries(${name} direct_bt catch2)
    add_dependencies(${name} direct_bt catch2)
    add_test (NAME ${name} COMMAND ${name})
endforeach()


