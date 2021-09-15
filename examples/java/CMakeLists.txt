# examples/java/CMakeLists.txt
find_package(Java REQUIRED)
include(UseJava)

add_jar(DBTScanner10 SOURCES DBTScanner10.java INCLUDE_JARS direct_bt_jar ENTRY_POINT Notification)
add_custom_command(TARGET DBTScanner10
        POST_BUILD
        COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/DBTScanner10.dir/DBTScanner10.class" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_dependencies(DBTScanner10 direct_bt_jar)

add_jar(DBTPeripheral00 SOURCES DBTPeripheral00.java INCLUDE_JARS direct_bt_jar ENTRY_POINT Notification)
add_custom_command(TARGET DBTPeripheral00
        POST_BUILD
        COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/DBTPeripheral00.dir/DBTPeripheral00.class" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_dependencies(DBTPeripheral00 direct_bt_jar)

