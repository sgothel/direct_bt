# java/CMakeLists.txt

set(direct_bt_jar_file ${CMAKE_CURRENT_BINARY_DIR}/direct_bt.jar CACHE FILEPATH "direct_bt jar file" FORCE)

set(CMAKE_JNI_TARGET TRUE)
file(GLOB_RECURSE JAVA_SOURCES "*.java")
set(CMAKE_JAVA_COMPILE_FLAGS ${direct_bt_JAVA_COMPILE_FLAGS})
add_jar(direct_bt_jar 
        ${JAVA_SOURCES}
        ${PROJECT_SOURCE_DIR}/jaulib/java_base/org/jau/util/BasicTypes.java
        ${PROJECT_SOURCE_DIR}/jaulib/java_jni/org/jau/sys/Clock.java
        ${PROJECT_SOURCE_DIR}/jaulib/java_jni/org/jau/io/PrintUtil.java
        ${PROJECT_SOURCE_DIR}/jaulib/java_net/org/jau/net/EUI48.java
        ${PROJECT_SOURCE_DIR}/jaulib/java_net/org/jau/net/EUI48Sub.java
        INCLUDE_JARS ${jaulib_fat_jar_file}
        MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/manifest.txt
        OUTPUT_NAME direct_bt
        GENERATE_NATIVE_HEADERS javadirect_bt_javah
            DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/direct_bt_jar.dir/jni"
)
add_dependencies(direct_bt_jar jaulib_fat_jar)

# JNI header generation is satisfied by above 'add_jar(.. GENERATE_NATIVE_HEADERS <target> DESTINATION <dir>)',
# note that its GENERATE_NATIVE_HEADERS target is a dummy target, since jni/direct_bt defines target javadirect_bt.
# Weakness of not directly checking build dependency of javadirect_bt against generated headers exists,
# however, it is unrealistic to assume that the transient generated JNI header will be edited manually
# within the process.
# Therefor we can use the dummy target javadirect_bt_javah and JNI header generation will only
# occur when java sources have been modified, i.e. the jar file being actually build.
#
#add_custom_command (TARGET direct_bt_jar
#  POST_BUILD
#  COMMAND ${CMAKE_COMMAND} -E echo "Generating JNI headers.."
#  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/direct_bt_jar.dir"
#  COMMAND ${JAVAC} -h jni/ ${JAVA_SOURCES}
#)

set(JNI_HEADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/direct_bt_jar.dir/jni")
install (FILES ${direct_bt_jar_file} DESTINATION ${CMAKE_INSTALL_LIBDIR}/../lib/java)

add_subdirectory (jni/direct_bt)

