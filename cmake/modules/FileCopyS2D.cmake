file(COPY ${_srcdir}/${_srcbasename} DESTINATION ${_dstdir})
file(RENAME ${_dstdir}/${_srcbasename} ${_dstdir}/${_dstbasename})
