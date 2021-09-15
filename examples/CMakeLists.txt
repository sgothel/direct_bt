include_directories(
    ${PROJECT_SOURCE_DIR}/jaulib/include
    ${PROJECT_SOURCE_DIR}/api
)

add_executable (dbt_scanner00 direct_bt_scanner00/dbt_scanner00.cpp)
target_link_libraries (dbt_scanner00 direct_bt)
add_dependencies(dbt_scanner00 direct_bt)

add_executable (dbt_scanner01 direct_bt_scanner01/dbt_scanner01.cpp)
target_link_libraries (dbt_scanner01 direct_bt)
add_dependencies(dbt_scanner01 direct_bt)

add_executable (dbt_scanner10 direct_bt_scanner10/dbt_scanner10.cpp)
target_link_libraries (dbt_scanner10 direct_bt)
add_dependencies(dbt_scanner10 direct_bt)

add_executable (dbt_peripheral00 direct_bt_peripheral00/dbt_peripheral00.cpp)
target_link_libraries (dbt_peripheral00 direct_bt)
add_dependencies(dbt_peripheral00 direct_bt)

install(TARGETS dbt_scanner00 dbt_scanner01 dbt_scanner10 dbt_peripheral00 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

