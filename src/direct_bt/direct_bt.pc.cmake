prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/lib@LIB_SUFFIX@
includedir=${prefix}/include/direct_bt

Name: direct_bt
Description: Direct-BT LE and BREDR library
Version: @direct_bt_VERSION_LONG@

Libs: -L${libdir} -ldirect_bt
Cflags: -I${includedir}
