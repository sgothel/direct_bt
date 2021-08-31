# examples/java/CMakeLists.txt
find_package(Java REQUIRED)
include(UseJava)

add_jar(DBTScanner10 SOURCES DBTScanner10.java INCLUDE_JARS direct_bt_jar ENTRY_POINT Notification)
add_custom_command(TARGET DBTScanner10
        POST_BUILD
        COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/DBTScanner10.dir/DBTScanner10.class" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_dependencies(DBTScanner10 direct_bt_jar)

