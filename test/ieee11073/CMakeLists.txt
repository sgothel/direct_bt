include_directories(
  ${PROJECT_SOURCE_DIR}/include/cppunit
  ${PROJECT_SOURCE_DIR}/api
)

add_executable (test_datatypes01     test_datatypes01.cpp)
target_link_libraries (test_datatypes01 direct_bt)

add_test (NAME datatypes01   COMMAND test_datatypes01)

