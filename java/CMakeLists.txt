find_package(Java REQUIRED)
find_package(JNI REQUIRED)
include(UseJava)

if (NOT DEFINED $ENV{JAVA_HOME_NATIVE})
  set (JAVA_HOME_NATIVE $ENV{JAVA_HOME})
  set (JAVAC $ENV{JAVA_HOME}/bin/javac)
  set (JAR $ENV{JAVA_HOME}/bin/jar)
else ()
  set (JAVAC $ENV{JAVA_HOME_NATIVE}/bin/javac)
  set (JAR $ENV{JAVA_HOME_NATIVE}/bin/jar)
endif ()

set(CMAKE_JNI_TARGET TRUE)
file(GLOB_RECURSE JAVA_SOURCES "*.java")
add_jar(tinybjar 
        ${JAVA_SOURCES}
        MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/manifest.txt
        OUTPUT_NAME tinyb2
        GENERATE_NATIVE_HEADERS javadirect_bt_javah
            DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/tinybjar.dir/jni"
)

# JNI header generation is satisfied by above 'add_jar(.. GENERATE_NATIVE_HEADERS <target> DESTINATION <dir>)',
# note that its GENERATE_NATIVE_HEADERS target is a dummy target, since jni/direct_bt defines target javadirect_bt.
# Weakness of not directly checking build dependency of javadirect_bt against generated headers exists,
# however, it is unrealistic to assume that the transient generated JNI header will be edited manually
# within the process.
# Therefor we can use the dummy target javadirect_bt_javah and JNI header generation will only
# occur when java sources have been modified, i.e. the jar file being actually build.
#
#add_custom_command (TARGET tinybjar
#  POST_BUILD
#  COMMAND ${CMAKE_COMMAND} -E echo "Generating JNI headers.."
#  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/tinybjar.dir"
#  COMMAND ${JAVAC} -h jni/ ${JAVA_SOURCES}
#)

set(JNI_HEADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/tinybjar.dir/jni")
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/tinyb2.jar DESTINATION ${CMAKE_INSTALL_LIBDIR}/../lib/java)

add_subdirectory (jni/direct_bt)

if(NOT SKIP_TINYB)
    add_subdirectory (jni/tinyb)
endif()
