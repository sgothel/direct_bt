find_package(JNI REQUIRED)

if (JNI_FOUND)
    message (STATUS "JNI_INCLUDE_DIRS=${JNI_INCLUDE_DIRS}")
    message (STATUS "JNI_LIBRARIES=${JNI_LIBRARIES}")
endif (JNI_FOUND)

set (direct_bt_LIB_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/api
    ${PROJECT_SOURCE_DIR}/api/direct_bt
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/java/jni
)

include_directories(
    ${JNI_INCLUDE_DIRS}
    ${direct_bt_LIB_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/jaulib/include
    ${CMAKE_BINARY_DIR}/java/${CMAKE_FILES_DIRECTORY}/direct_bt_jar.dir/jni
    ${CMAKE_BINARY_DIR}/jaulib/java_jni/${CMAKE_FILES_DIRECTORY}/jaulib_jni_jar.dir/jni
    ${JNI_HEADER_PATH}
)

set (direct_bt_JNI_SRCS
  ${PROJECT_SOURCE_DIR}/jaulib/java_jni/jni/jni_mem.cxx
  ${PROJECT_SOURCE_DIR}/jaulib/java_jni/jni/helper_jni.cxx
  ${PROJECT_SOURCE_DIR}/jaulib/java_jni/jni/jau/jau_sys_Clock.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/helper_base.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/helper_dbt.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/BTFactory.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTAdapter.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTDevice.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTGattChar.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTGattDesc.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTGattService.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTManager.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTNativeDownlink.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTObject.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBGattServer.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/EInfoReport.cxx
)

set (CMAKE_SHARED_LINKER_FLAGS "-Wl,--as-needed")

add_library (javadirect_bt SHARED ${direct_bt_JNI_SRCS})
target_compile_options(javadirect_bt PUBLIC ${direct_bt_CXX_FLAGS})
target_link_options(javadirect_bt PUBLIC ${direct_bt_SHARED_LINKER_FLAGS})
target_link_libraries(javadirect_bt ${JNI_LIBRARIES} direct_bt)

if(USE_STRIP)
add_custom_command(TARGET javadirect_bt POST_BUILD
                   COMMAND ${STRIP} ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}javadirect_bt${CMAKE_SHARED_LIBRARY_SUFFIX}.${direct_bt_VERSION_SHORT}
                   COMMENT "stripping javadirect_bt"
                   VERBATIM
                   )
endif(USE_STRIP)

set_target_properties(
    javadirect_bt
    PROPERTIES
    SOVERSION ${direct_bt_VERSION_MAJOR}
    VERSION ${direct_bt_VERSION_SHORT}
)

install(TARGETS javadirect_bt LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_dependencies(javadirect_bt direct_bt direct_bt_jar)

