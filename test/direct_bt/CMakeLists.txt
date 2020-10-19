include_directories(
  ${PROJECT_SOURCE_DIR}/include/cppunit
  ${PROJECT_SOURCE_DIR}/jaucpp/include
  ${PROJECT_SOURCE_DIR}/api
)

add_executable (test_basictypes01    test_basictypes01.cpp)
target_link_libraries (test_basictypes01 direct_bt)
add_dependencies(test_basictypes01 direct_bt)
add_test (NAME basictypes01   COMMAND test_basictypes01)

add_executable (test_uuid            test_uuid.cpp)
target_link_libraries (test_uuid direct_bt)
add_dependencies(test_uuid direct_bt)
add_test (NAME uuid           COMMAND test_uuid)

add_executable (test_attpdu01        test_attpdu01.cpp)
target_link_libraries (test_attpdu01 direct_bt)
add_dependencies(test_attpdu01 direct_bt)
add_test (NAME attpdu01       COMMAND test_attpdu01)

