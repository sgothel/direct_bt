# java/CMakeLists.txt

find_jar(JUNIT_JAR
  NAMES junit4 junit
  PATHS "/usr/share/java" "/usr/local/share/java/classes" )

find_jar(HAMCREST_JAR
  NAMES hamcrest-all hamcrest
  PATHS "/usr/share/java" "/usr/local/share/java/classes" )

set(direct_bt_test_jar_file ${CMAKE_CURRENT_BINARY_DIR}/direct_bt-test.jar CACHE FILEPATH "direct_bt test jar file" FORCE)

file(GLOB_RECURSE TEST_JAVA_SOURCES "*.java")

file(GLOB_RECURSE TEST_JAVA_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "Test*.java")

set(CMAKE_JAVA_COMPILE_FLAGS ${direct_bt_JAVA_COMPILE_FLAGS})

add_jar(direct_bt_test
        ${TEST_JAVA_SOURCES}
        INCLUDE_JARS ${jaulib_fat_jar_file} ${direct_bt_fat_jar_file} ${jaulib_test_jar_file} ${JUNIT_JAR} ${HAMCREST_JAR}
        MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/manifest.txt
        OUTPUT_NAME direct_bt-test
)
add_dependencies(direct_bt_test direct_bt_fat_jar jaulib_test)

install (FILES ${direct_bt_test_jar_file} DESTINATION ${CMAKE_INSTALL_LIBDIR}/../lib/java)

string( REPLACE ".java" "" TEST_JAVA_FILES2 "${TEST_JAVA_FILES}" )
string( REPLACE "/" "." BASENAMES_IDIOMATIC_EXAMPLES "${TEST_JAVA_FILES2}" )
set( TARGETS_IDIOMATIC_EXAMPLES ${BASENAMES_IDIOMATIC_EXAMPLES} )

foreach(name ${TARGETS_IDIOMATIC_EXAMPLES})
    add_test (NAME ${name} COMMAND ${JAVA_RUNTIME} 
                           -cp ${JUNIT_JAR}:${HAMCREST_JAR}:${direct_bt_fat_jar_file}:${jaulib_test_jar_file}:${direct_bt_test_jar_file}
                           org.junit.runner.JUnitCore ${name})
endforeach()



