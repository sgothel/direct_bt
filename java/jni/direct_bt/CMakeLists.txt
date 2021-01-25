find_package(JNI REQUIRED)

if (JNI_FOUND)
    message (STATUS "JNI_INCLUDE_DIRS=${JNI_INCLUDE_DIRS}")
    message (STATUS "JNI_LIBRARIES=${JNI_LIBRARIES}")
endif (JNI_FOUND)

set (direct_bt_LIB_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/jaulib/include
    ${PROJECT_SOURCE_DIR}/api
    ${PROJECT_SOURCE_DIR}/api/direct_bt
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/java/jni
)

include_directories(
    ${JNI_INCLUDE_DIRS}
    ${direct_bt_LIB_INCLUDE_DIRS}
    ${JNI_HEADER_PATH}
)

set (direct_bt_JNI_SRCS
  ${PROJECT_SOURCE_DIR}/jaulib/java_jni/jni/jni_mem.cxx
  ${PROJECT_SOURCE_DIR}/jaulib/java_jni/jni/helper_jni.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/helper_base.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/BluetoothFactory.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/BluetoothUtils.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/helper_dbt.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTNativeDownlink.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTAdapter.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTDevice.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTEvent.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTGattCharacteristic.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTGattDescriptor.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTGattService.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTManager.cxx
  ${PROJECT_SOURCE_DIR}/java/jni/direct_bt/DBTObject.cxx
)

set (CMAKE_SHARED_LINKER_FLAGS "-Wl,--as-needed")

add_library (javadirect_bt SHARED ${direct_bt_JNI_SRCS})
target_link_libraries(javadirect_bt ${JNI_LIBRARIES} direct_bt)

set_target_properties(
    javadirect_bt
    PROPERTIES
    SOVERSION ${direct_bt_VERSION_MAJOR}
    VERSION ${direct_bt_VERSION_STRING}
)

install(TARGETS javadirect_bt LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_dependencies(javadirect_bt direct_bt direct_bt_jar)

