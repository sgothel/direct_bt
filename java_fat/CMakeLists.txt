# java/CMakeLists.txt

#                    COMMAND ${CMAKE_COMMAND} 
#                            -DOS_AND_ARCH=${OS_AND_ARCH}
#                            -Dos_and_arch_slash=${os_and_arch_slash}
#                            -Dos_and_arch_dot=${os_and_arch_dot}
#                            -D_jarfile=${CMAKE_CURRENT_BINARY_DIR}/jaulib_jni-natives-${OS_AND_ARCH}.jar
#                            -D_workdir=${CMAKE_CURRENT_BINARY_DIR}
#                            -D_module=jni
#                            -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/MakeNativeJar.cmake

# set(direct_bt_jar_file ${CMAKE_CURRENT_BINARY_DIR}/direct_bt.jar CACHE FILEPATH "direct_bt jar file" FORCE)
set(direct_bt_fat_jar_file ${CMAKE_CURRENT_BINARY_DIR}/direct_bt-fat.jar CACHE FILEPATH "direct_bt fat jar file" FORCE)
set(direct_bt_java_src_file ${CMAKE_CURRENT_BINARY_DIR}/direct_bt-java-src.zip CACHE FILEPATH "direct_bt java source file" FORCE)

add_custom_command (OUTPUT ${direct_bt_fat_jar_file} ${direct_bt_java_src_file}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    COMMAND ${CMAKE_COMMAND}
                            -D_srcdir=${CMAKE_CURRENT_BINARY_DIR}/../src/direct_bt
                            -D_dstdir=${CMAKE_CURRENT_BINARY_DIR}/natives/${OS_AND_ARCH}
                            -D_srcbasename=${CMAKE_SHARED_LIBRARY_PREFIX}direct_bt${CMAKE_SHARED_LIBRARY_SUFFIX}.${direct_bt_VERSION_SHORT}
                            -D_dstbasename=${CMAKE_SHARED_LIBRARY_PREFIX}direct_bt${CMAKE_SHARED_LIBRARY_SUFFIX}
                            -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/FileCopyS2D.cmake
                    COMMAND ${CMAKE_COMMAND}
                            -D_srcdir=${CMAKE_CURRENT_BINARY_DIR}/../java/jni/direct_bt
                            -D_dstdir=${CMAKE_CURRENT_BINARY_DIR}/natives/${OS_AND_ARCH}
                            -D_srcbasename=${CMAKE_SHARED_LIBRARY_PREFIX}javadirect_bt${CMAKE_SHARED_LIBRARY_SUFFIX}.${direct_bt_VERSION_SHORT}
                            -D_dstbasename=${CMAKE_SHARED_LIBRARY_PREFIX}javadirect_bt${CMAKE_SHARED_LIBRARY_SUFFIX}
                            -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/FileCopyS2D.cmake
                    COMMAND ${CMAKE_COMMAND}
                            -D _dstdir:PATH='${CMAKE_CURRENT_BINARY_DIR}/temp'
                            -D _archives:FILEPATH="${jaulib_fat_jar_file}"
                            -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/ExtractArchiveList.cmake

                    COMMAND ${CMAKE_COMMAND}
                            -D_srcdir=${CMAKE_CURRENT_BINARY_DIR}/../java/${CMAKE_FILES_DIRECTORY}/direct_bt_jar.dir/jau
                            -D_dstdir=${CMAKE_CURRENT_BINARY_DIR}/temp
                            -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/DirCopy.cmake
                    COMMAND ${CMAKE_COMMAND}
                            -D_srcdir=${CMAKE_CURRENT_BINARY_DIR}/../java/${CMAKE_FILES_DIRECTORY}/direct_bt_jar.dir/org
                            -D_dstdir=${CMAKE_CURRENT_BINARY_DIR}/temp
                            -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/DirCopy.cmake

                    COMMAND ${JAR}
                       --create --file ${direct_bt_fat_jar_file}
                       --manifest ${CMAKE_CURRENT_BINARY_DIR}/manifest-fat.txt
                       -C ${CMAKE_CURRENT_BINARY_DIR}/temp/ jau
                       -C ${CMAKE_CURRENT_BINARY_DIR}/temp/ org
                       -C ${CMAKE_CURRENT_BINARY_DIR}/temp/ natives
                       -C ${CMAKE_CURRENT_BINARY_DIR}/ natives

                    COMMAND ${JAR}
                       --create --file ${direct_bt_java_src_file}
                       --no-manifest
                       -C ${CMAKE_CURRENT_SOURCE_DIR}/../java jau
                       -C ${CMAKE_CURRENT_SOURCE_DIR}/../java org
                    DEPENDS jaulib_fat_jar direct_bt direct_bt_jar javadirect_bt
                    COMMENT "producing direct_bt [fat] jar files and its java source zip"
                    VERBATIM
                   )

add_custom_target (direct_bt_fat_jar ALL 
                   DEPENDS ${direct_bt_fat_jar_file} ${direct_bt_java_src_file}
                  )

add_dependencies(direct_bt_fat_jar jaulib_fat_jar direct_bt direct_bt_jar javadirect_bt)

install (FILES ${direct_bt_fat_jar_file} ${direct_bt_java_src_file} DESTINATION ${CMAKE_INSTALL_LIBDIR}/../lib/java)


