# java/CMakeLists.txt

find_jar(JUNIT_JAR
  NAMES junit4 junit
  PATHS "/usr/share/java")

set(direct_bt_trial_jar_file ${CMAKE_CURRENT_BINARY_DIR}/direct_bt-trial.jar CACHE FILEPATH "direct_bt trial jar file" FORCE)

file(GLOB_RECURSE TEST_JAVA_SOURCES "*.java")

file(GLOB_RECURSE TEST_JAVA_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "Test*.java")

add_jar(direct_bt_trial
        ${TEST_JAVA_SOURCES}
        INCLUDE_JARS ${direct_bt_fat_jar_file} ${jaulib_base_jar_file} ${jaulib_test_jar_file} ${JUNIT_JAR}
        MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/manifest.txt
        OUTPUT_NAME direct_bt-trial
)
add_dependencies(direct_bt_trial direct_bt_fat_jar jaulib_test)

install (FILES ${direct_bt_trial_jar_file} DESTINATION ${CMAKE_INSTALL_LIBDIR}/../lib/java)

string( REPLACE ".java" "" TEST_JAVA_FILES2 "${TEST_JAVA_FILES}" )
string( REPLACE "/" "." BASENAMES_IDIOMATIC_EXAMPLES "${TEST_JAVA_FILES2}" )
set( TARGETS_IDIOMATIC_EXAMPLES ${BASENAMES_IDIOMATIC_EXAMPLES} )

#foreach(name ${TARGETS_IDIOMATIC_EXAMPLES})
#    add_test (NAME ${name} COMMAND ${JAVA_RUNTIME} 
#                           -cp ${JUNIT_JAR}:${direct_bt_fat_jar_file}:${jaulib_trial_jar_file}:${direct_bt_trial_jar_file}
#                           org.junit.runner.JUnitCore ${name})
#endforeach()

foreach(name ${TARGETS_IDIOMATIC_EXAMPLES})
    add_test (NAME ${name} COMMAND sudo /sbin/capsh --caps=cap_net_raw,cap_net_admin+eip\ cap_setpcap,cap_setuid,cap_setgid+ep
                           --keep=1 --user=$ENV{USER} --addamb=cap_net_raw,cap_net_admin+eip
                           -- -c "ulimit -c unlimited; $EXE_WRAPPER ${JAVA_RUNTIME} -cp ${JUNIT_JAR}:${direct_bt_jar_file}:${jaulib_base_jar_file}:${jaulib_test_jar_file}:${direct_bt_trial_jar_file} -Djava.library.path=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR} org.junit.runner.JUnitCore ${name}")
endforeach()

