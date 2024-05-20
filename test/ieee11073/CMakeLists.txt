include_directories(
  ${PROJECT_SOURCE_DIR}/jaulib/include
  ${PROJECT_SOURCE_DIR}/api
)

# These examples use the standard separate compilation
set( SOURCES_IDIOMATIC_EXAMPLES
  test_ieee11073_01.cpp
)

string( REPLACE ".cpp" "" BASENAMES_IDIOMATIC_EXAMPLES "${SOURCES_IDIOMATIC_EXAMPLES}" )
set( TARGETS_IDIOMATIC_EXAMPLES ${BASENAMES_IDIOMATIC_EXAMPLES} )

foreach( name ${TARGETS_IDIOMATIC_EXAMPLES} )
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} direct_bt catch2)
    add_dependencies(${name} direct_bt catch2)
    add_test (NAME ${name} COMMAND ${name})
    if(DEFINED CMAKE_CXX_CLANG_TIDY)
        set_target_properties(${name} PROPERTIES CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY};--checks;-bugprone-chained-comparison")
    endif()
endforeach()

