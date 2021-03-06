# examples/java/CMakeLists.txt
find_package(Java REQUIRED)
include(UseJava)

add_jar(HelloTinyB SOURCES HelloTinyB.java INCLUDE_JARS direct_bt_jar ENTRY_POINT HelloTinyB)
add_custom_command(TARGET HelloTinyB
        POST_BUILD
        COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/HelloTinyB.dir/HelloTinyB.class" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_dependencies(HelloTinyB direct_bt_jar)

add_jar(AsyncTinyB SOURCES AsyncTinyB.java INCLUDE_JARS direct_bt_jar ENTRY_POINT AsyncTinyB)
add_custom_command(TARGET AsyncTinyB
        POST_BUILD
        COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/AsyncTinyB.dir/AsyncTinyB.class" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_dependencies(AsyncTinyB direct_bt_jar)

add_jar(Notification SOURCES Notification.java INCLUDE_JARS direct_bt_jar ENTRY_POINT Notification)
add_custom_command(TARGET Notification
        POST_BUILD
        COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/Notification.dir/Notification.class" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_dependencies(Notification direct_bt_jar)

add_jar(ScannerTinyB00 SOURCES ScannerTinyB00.java INCLUDE_JARS direct_bt_jar ENTRY_POINT Notification)
add_custom_command(TARGET ScannerTinyB00
        POST_BUILD
        COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/ScannerTinyB00.dir/ScannerTinyB00.class" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_dependencies(ScannerTinyB00 direct_bt_jar)

add_jar(ScannerTinyB01 SOURCES ScannerTinyB01.java INCLUDE_JARS direct_bt_jar ENTRY_POINT Notification)
add_custom_command(TARGET ScannerTinyB01
        POST_BUILD
        COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/ScannerTinyB01.dir/ScannerTinyB01.class" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_dependencies(ScannerTinyB01 direct_bt_jar)

add_jar(ScannerTinyB02 SOURCES ScannerTinyB02.java INCLUDE_JARS direct_bt_jar ENTRY_POINT Notification)
add_custom_command(TARGET ScannerTinyB02
        POST_BUILD
        COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/ScannerTinyB02.dir/ScannerTinyB02.class" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_dependencies(ScannerTinyB02 direct_bt_jar)

add_jar(DBTScanner10 SOURCES DBTScanner10.java INCLUDE_JARS direct_bt_jar ENTRY_POINT Notification)
add_custom_command(TARGET DBTScanner10
        POST_BUILD
        COMMAND cp "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/DBTScanner10.dir/DBTScanner10.class" "${CMAKE_CURRENT_BINARY_DIR}"
)
add_dependencies(DBTScanner10 direct_bt_jar)

