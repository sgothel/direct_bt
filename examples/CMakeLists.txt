include_directories(
    ${PROJECT_SOURCE_DIR}/jaulib/include
    ${PROJECT_SOURCE_DIR}/api
)

#include_directories(
#  ${PROJECT_SOURCE_DIR}/api
#  ${direct_bt_LIB_INCLUDE_DIRS}
#  ${tinyb_LIB_INCLUDE_DIRS}
#  ${GLIB2_INCLUDE_DIRS}
#  ${GIO_INCLUDE_DIRS}
#  ${GIO-UNIX_INCLUDE_DIRS}
#)

if(NOT SKIP_TINYB)

    add_executable (hellotinyb tinyb/hellotinyb.cpp)
    target_link_libraries (hellotinyb tinyb)
    add_dependencies(hellotinyb tinyb)

    add_executable (checkinit tinyb/checkinit.cpp)
    target_link_libraries (checkinit tinyb)
    add_dependencies(checkinit tinyb)

    add_executable (asynctinyb tinyb/asynctinyb.cpp)
    target_link_libraries (asynctinyb tinyb)
    add_dependencies(asynctinyb tinyb)

    add_executable (esstinyb tinyb/esstinyb.cpp)
    target_link_libraries (esstinyb tinyb)
    add_dependencies(esstinyb tinyb)

    add_executable (notifications tinyb/notifications.cpp)
    target_link_libraries (notifications tinyb)
    add_dependencies(notifications tinyb)

    add_executable (uuid tinyb/uuid.cpp)
    target_link_libraries (uuid tinyb)
    add_dependencies(uuid tinyb)

    add_executable (list_mfg tinyb/list_mfg.cpp)
    target_link_libraries (list_mfg tinyb)
    add_dependencies(list_mfg tinyb)

endif()

add_executable (dbt_scanner00 direct_bt_scanner00/dbt_scanner00.cpp)
target_link_libraries (dbt_scanner00 direct_bt)
add_dependencies(dbt_scanner00 direct_bt)

add_executable (dbt_scanner01 direct_bt_scanner01/dbt_scanner01.cpp)
target_link_libraries (dbt_scanner01 direct_bt)
add_dependencies(dbt_scanner01 direct_bt)

add_executable (dbt_scanner10 direct_bt_scanner10/dbt_scanner10.cpp)
target_link_libraries (dbt_scanner10 direct_bt)
add_dependencies(dbt_scanner10 direct_bt)

install(TARGETS dbt_scanner00 dbt_scanner01 dbt_scanner10 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

