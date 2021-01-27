message(STATUS "rm -rf ${_dstdir}")
file(REMOVE_RECURSE ${_dstdir})

# list(LENGTH _archives len)
# message(STATUS "X1: <${_archives}>, len ${len}")

string(REPLACE "\"" "" l1 ${_archives})
string(REPLACE " " ";" l2 ${l1})

# list(LENGTH l2 len)
# message(STATUS "X2: <${l2}>, len ${len}")

foreach(item ${l2})
    if(${CMAKE_VERSION} VERSION_LESS "3.18.0")
        message(STATUS "unzip-native ${item} -> ${_dstdir}")
        execute_process(COMMAND unzip -qq ${item} -d ${_dstdir})
    else()
        message(STATUS "unzip-cmake ${item} -> ${_dstdir}")
        file(ARCHIVE_EXTRACT INPUT ${item} DESTINATION ${_dstdir})
    endif()
endforeach()

unset(l1)
unset(l2)
unset(item)
