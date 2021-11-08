include_directories(
    ${PROJECT_SOURCE_DIR}/jaulib/include
    ${PROJECT_SOURCE_DIR}/api
)

# These examples use the standard separate compilation
set( SOURCES_IDIOMATIC_EXAMPLES
    dbt_readsmpkey.cpp
    dbt_scanner00.cpp
    dbt_scanner01.cpp
    dbt_scanner10.cpp
    dbt_peripheral00.cpp
    dbt_test_gattcharlifecycle01.cpp
)

string( REPLACE ".cpp" "" BASENAMES_IDIOMATIC_EXAMPLES "${SOURCES_IDIOMATIC_EXAMPLES}" )
set( TARGETS_IDIOMATIC_EXAMPLES ${BASENAMES_IDIOMATIC_EXAMPLES} )

foreach( name ${TARGETS_IDIOMATIC_EXAMPLES} )
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} direct_bt)
    add_dependencies(${name} direct_bt)
endforeach()

install(TARGETS ${TARGETS_IDIOMATIC_EXAMPLES} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

