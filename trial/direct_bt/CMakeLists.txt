include_directories(
  ${PROJECT_SOURCE_DIR}/jaulib/include
  ${PROJECT_SOURCE_DIR}/jaulib/include/catch2_jau
  ${PROJECT_SOURCE_DIR}/api
)

# These examples use the standard separate compilation
file(GLOB_RECURSE SOURCES_IDIOMATIC_TRIALS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "test_*.cpp")
string( REPLACE ".cpp" "" TARGETS_IDIOMATIC_TRIALS "${SOURCES_IDIOMATIC_TRIALS}" )

foreach( name ${TARGETS_IDIOMATIC_TRIALS} )
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} direct_bt catch2)
    add_dependencies(${name} direct_bt catch2)
    add_test (NAME ${name} COMMAND sudo -E /sbin/capsh --caps=cap_net_raw,cap_net_admin+eip\ cap_setpcap,cap_setuid,cap_setgid+ep
                           --keep=1 --user=$ENV{USER} --addamb=cap_net_raw,cap_net_admin+eip
                           -- -c "ulimit -c unlimited; ./${name}")
    if(DEFINED CMAKE_CXX_CLANG_TIDY)
        set_target_properties(${name} PROPERTIES CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY};--checks;-bugprone-chained-comparison")
    endif()
endforeach()

file(GLOB_RECURSE SOURCES_IDIOMATIC_PROVOCATIONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "provoke_*.cpp")
string( REPLACE ".cpp" "" TARGETS_IDIOMATIC_PROVOCATIONS "${SOURCES_IDIOMATIC_PROVOCATIONS}" )

foreach( name ${TARGETS_IDIOMATIC_PROVOCATIONS} )
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} direct_bt catch2)
    add_dependencies(${name} direct_bt catch2)
endforeach()

