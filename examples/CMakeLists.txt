include_directories(
    ${PROJECT_SOURCE_DIR}/jaulib/include
    ${PROJECT_SOURCE_DIR}/api
)

# These examples use the standard separate compilation
file(GLOB_RECURSE SOURCES_IDIOMATIC_EXAMPLES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "dbt_*.cpp")

string( REPLACE ".cpp" "" BASENAMES_IDIOMATIC_EXAMPLES "${SOURCES_IDIOMATIC_EXAMPLES}" )
set( TARGETS_IDIOMATIC_EXAMPLES ${BASENAMES_IDIOMATIC_EXAMPLES} )

foreach( name ${TARGETS_IDIOMATIC_EXAMPLES} )
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} direct_bt)
    add_dependencies(${name} direct_bt)
endforeach()

install(TARGETS ${TARGETS_IDIOMATIC_EXAMPLES} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

