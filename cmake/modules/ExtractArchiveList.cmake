message(STATUS "rm -rf ${_dstdir}")
file(REMOVE_RECURSE ${_dstdir})

# list(LENGTH _archives len)
# message(STATUS "X1: <${_archives}>, len ${len}")

string(REPLACE "\"" "" l1 ${_archives})
string(REPLACE " " ";" l2 ${l1})

# list(LENGTH l2 len)
# message(STATUS "X2: <${l2}>, len ${len}")

foreach(item ${l2})
    message(STATUS "unzip ${item} -> ${_dstdir}")
    file(ARCHIVE_EXTRACT INPUT ${item} DESTINATION ${_dstdir})
endforeach()

unset(l1)
unset(l2)
unset(item)
