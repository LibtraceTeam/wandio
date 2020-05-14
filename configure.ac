# Process this file with autoconf to produce a configure script.

# Now you only need to update the version number in two places - below,
# and in the README

AC_INIT([wandio],[4.2.3],[contact@wand.net.nz],[wandio])

WANDIO_MAJOR=4
WANDIO_MID=2
WANDIO_MINOR=3

# OpenSolaris hides libraries like libncurses in /usr/gnu/lib, which is not
# searched by default - add it to LDFLAGS so we at least have a chance of
# finding it
if test -d "/usr/gnu/lib"; then
	LDFLAGS+=" -L/usr/gnu/lib"
fi

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR(lib/wandio.c)
AM_INIT_AUTOMAKE([subdir-objects])

# Make sure we use the relatively silent automake output
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Define our wandio version number externally so we can use it in the source
# as well, if needed.
AC_DEFINE([WANDIO_MAJOR],${WANDIO_MAJOR},[wandio major version])
AC_DEFINE([WANDIO_MID],${WANDIO_MID},[wandio mid version])
AC_DEFINE([WANDIO_MINOR],${WANDIO_MINOR},[wandio minor version])

# These are all the files we want to be built for us by configure
AC_CONFIG_FILES([Makefile lib/Makefile tools/wandiocat/Makefile])


# Function that checks if the C++ compiler actually works - there's a bit of
# oversight in autoconf that will set the C++ compiler to g++ if no compiler
# is found, even if g++ is not present! So we need an extra test to make sure
# that the compiler works :(

AC_DEFUN([rw_PROG_CXX_WORKS],
[AC_REQUIRE([AC_PROG_CXX])dnl
AC_CACHE_CHECK([whether the C++ compiler works],
	[rw_cv_prog_cxx_works],
	[AC_LANG_PUSH([C++])
	AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
		[rw_cv_prog_cxx_works=yes],
		[rw_cv_prog_cxx_works=no])
	AC_LANG_POP([C++])])
])

# Put all our automake definitions in config.h
AM_CONFIG_HEADER([config.h])

# Checks for C and C++ compilers
AC_PROG_CC
AC_PROG_CXX
rw_PROG_CXX_WORKS

if test "$rw_cv_prog_cxx_works" = "no"; then
	AC_MSG_ERROR("Failed to find working C++ compiler")
fi

# Checking for 'install'
AC_PROG_INSTALL

# All our source files for function replacements are in lib/
AC_CONFIG_LIBOBJ_DIR(lib)

# Check for various "replacement" functions
AC_FUNC_MALLOC
AC_FUNC_REALLOC


# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_SYS_LARGEFILE

ADD_INCLS=""
ADD_LDFLAGS="$ADD_LDFLAGS -L\$(abs_top_srcdir)/lib"
LIBWANDIO_LIBS=""

CFLAGS="$CFLAGS -Wall -Wmissing-prototypes -Wextra -DLT_BUILDING_DLL=1"
CXXFLAGS="$CXXFLAGS -Wall -DLT_BUILDING_DLL=1"

# Check for -fvisibility
gl_VISIBILITY

gcc_PACKED
gcc_DEPRECATED
gcc_UNUSED
gcc_PURE
gcc_FORMAT

# Check for libtool
AC_PROG_LIBTOOL

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL

# Fail if any of these functions are missing
#AC_CHECK_FUNCS()

AC_CHECK_SIZEOF([long int])


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(stddef.h inttypes.h sys/prctl.h)

# Checks for various "optional" libraries
AC_CHECK_LIB(pthread, pthread_create, have_pthread=1, have_pthread=0)

if test "$have_pthread" = 1; then
	LIBWANDIO_LIBS="$LIBWANDIO_LIBS -lpthread"
	AC_DEFINE(HAVE_LIBPTHREAD, 1, [Set to 1 if pthreads are supported])
fi

# Stupid AC_SEARCH_LIB appends stuff to LIBS even when I don't want it to, so
# just set libs to null here to avoid linking against them by default
LIBS=

AC_ARG_WITH([bzip2],
	AC_HELP_STRING([--with-bzip2], [build with support for bzip2 compressed files]))

AS_IF([test "x$with_bzip2" != "xno"],
	[
	AC_CHECK_LIB(bz2, BZ2_bzDecompressInit, have_bzip=yes, have_bzip=no)
	], [have_bzip=no])

AS_IF([test "x$have_bzip" = "xyes"], [
	if test "$ac_cv_lib_bz2_BZ2_bzDecompressInit" != "none required"; then
		LIBWANDIO_LIBS="$LIBWANDIO_LIBS -lbz2"
	fi
	with_bzip2=yes
	AC_DEFINE(HAVE_LIBBZ2, 1, "Compiled with bzip2 support")],

	[AS_IF([test "x$with_bzip2" = "xyes"],
		[AC_MSG_ERROR([bzip2 requested but not found])])
	AC_DEFINE(HAVE_LIBBZ2, 0, "Compiled with bzip2 support")
	with_bzip2=no]
)

AC_ARG_WITH([zlib],
	AC_HELP_STRING([--with-zlib], [build with support for zlib compressed files]))

AS_IF([test "x$with_zlib" != "xno"],
	[
	AC_CHECK_LIB(z, deflate, have_zlib=yes, have_zlib=no)
	], [have_zlib=no])

AS_IF([test "x$have_zlib" = "xyes"], [
	if test "$ac_cv_lib_z_deflate" != "none required"; then
		LIBWANDIO_LIBS="$LIBWANDIO_LIBS -lz"
	fi
	AC_DEFINE(HAVE_LIBZ, 1, "Compiled with zlib support")
	with_zlib=yes],


	[AS_IF([test "x$with_zlib" = "xyes"],
		[AC_MSG_ERROR([zlib requested but not found])])
	AC_DEFINE(HAVE_LIBZ, 0, "Compiled with zlib support")
	with_zlib=no]
)

AC_ARG_WITH([lzo],
	AC_HELP_STRING([--with-lzo], [build with support for writing lzo compressed files]))

AS_IF([test "x$with_lzo" != "xno"],
	[
	AC_CHECK_LIB(lzo2, lzo1x_1_compress, have_lzo=yes, have_lzo=no)
	], [have_lzo=no])

AS_IF([test "x$have_lzo" = "xyes"], [
	if test "$ac_cv_lib_lzo2_lzo1x_1_compress" != "none required"; then
		LIBWANDIO_LIBS="$LIBWANDIO_LIBS -llzo2"
	fi
	AC_DEFINE(HAVE_LIBLZO2, 1, "Compiled with lzo2 support")
	with_lzo=yes],


	[AS_IF([test "x$with_lzo" = "xyes"],
		[AC_MSG_ERROR([lzo requested but not found])])
	AC_DEFINE(HAVE_LIBLZO2, 0, "Compiled with lzo2 support")
	with_lzo=no]
)

AC_ARG_WITH([lzma],
	AC_HELP_STRING([--with-lzma], [build with support for lzma compressed files]))

AS_IF([test "x$with_lzma" != "xno"],
	[
	AC_CHECK_HEADER(lzma.h, have_lzma=yes, have_lzma=no)
	], [have_lzma=no])

AS_IF([test "x$have_lzma" = "xyes"], [
	if test "$ac_cv_lib_lzma_code" != "none required"; then
		LIBWANDIO_LIBS="$LIBWANDIO_LIBS -llzma"
	fi
	AC_DEFINE(HAVE_LIBLZMA, 1, "Compiled with lzma support")
	with_lzma=yes],


	[AS_IF([test "x$with_lzma" = "xyes"],
		[AC_MSG_ERROR([lzma requested but not found])])
	AC_DEFINE(HAVE_LIBLZMA, 0, "Compiled with lzma support")
	with_lzma=no]
)

AC_ARG_WITH([zstd],
	AC_HELP_STRING([--with-zstd], [build with support for zstd compressed files]))

AS_IF([test "x$with_zstd" != "xno"],
	[
	AC_CHECK_LIB(zstd, ZSTD_createDStream, have_zstd=yes, have_zstd=no)
	], [have_zstd=no])

AS_IF([test "x$have_zstd" = "xyes"], [
	if test "$ac_cv_lib_zstd_code" != "none required"; then
		LIBWANDIO_LIBS="$LIBWANDIO_LIBS -lzstd"
	fi
	AC_DEFINE(HAVE_LIBZSTD, 1, "Compiled with zstd support")
	with_zstd=yes],


	[AS_IF([test "x$with_zstd" = "xyes"],
		[AC_MSG_ERROR([zstd requested but not found])])
	AC_DEFINE(HAVE_LIBZSTD, 0, "Compiled with zstd support")
	with_zstd=no]
)

AC_ARG_WITH([qatzip],
	AC_HELP_STRING([--with-qatzip], [build with support for Intel QuickAssist compression]))

AS_IF([test "x$with_qatzip" != "xno"],
	[
	AC_CHECK_LIB(qatzip, qzCompress, have_qatzip=yes, have_qatzip=no)
	], [have_qatzip=no])

AS_IF([test "x$have_qatzip" = "xyes"], [
	if test "$ac_cv_lib_qatzip_qzCompress" != "none required"; then
		LIBWANDIO_LIBS="$LIBWANDIO_LIBS -lqatzip"
	fi
	AC_DEFINE(HAVE_LIBQATZIP, 1, "Compiled with QATzip support")
	with_qatzip=yes],


	[AS_IF([test "x$with_qatzip" = "xyes"],
		[AC_MSG_ERROR([QATzip requested but not found])])
	AC_DEFINE(HAVE_LIBQATZIP, 0, "Compiled with QATzip support")
	with_qatzip=no]
)

AC_ARG_WITH([lz4],
        AC_HELP_STRING([--with-lz4], [build with support for lz4 compressed files]))

AS_IF([test "x$with_lz4" != "xno"],
        [
        AC_CHECK_LIB(lz4, LZ4F_createDecompressionContext, have_lz4f=yes, have_lz4f=no)
        ], [have_zstd=no])

AS_IF([test "x$have_lz4f" = "xyes"], [
        if test "$ac_cv_lib_lz4_code" != "none required"; then
                LIBWANDIO_LIBS="$LIBWANDIO_LIBS -llz4"
        fi
        AC_DEFINE(HAVE_LIBLZ4F, 1, "Compiled with lz4 frame support")
        AC_CHECK_LIB(lz4, LZ4F_getVersion, have_lz4_173=yes, have_lz4_173=no)
        if test "x$have_lz4_173" = "xyes"; then
                AC_DEFINE(HAVE_LIBLZ4_MOVABLE, 1, "If defined then liblz4 does NOT have the ERROR_srcPtr_wrong bug")
        fi
        with_lz4=frameapi],
        [
            AC_DEFINE(HAVE_LIBLZ4F, 0, "Compiled with lz4 frame support")
            AC_CHECK_LIB(lz4, LZ4_decompress_safe_continue, have_lz4s=yes, have_lz4s=no)
            AS_IF([test "x$have_lz4s" = "xyes"], [
                if test "$ac_cv_lib_lz4_code" != "none required"; then
                    LIBWANDIO_LIBS="$LIBWANDIO_LIBS -llz4"
                fi
                AC_DEFINE(HAVE_LIBLZ4S, 1, "Compiled with lz4 stream support")
                with_lz4=streamapi
            ], [
                AC_DEFINE(HAVE_LIBLZ4S, 0, "Compiled with lz4 stream support")
                with_lz4=no])

            AS_IF([test "x$with_lz4" = "xyes"], [
                    AC_MSG_ERROR([lz4 requested but frame API was not found])])
        ]
)

AC_ARG_WITH([http],
        AC_HELP_STRING([--with-http], [build with support for reading files over http (using libcurl)]))

AS_IF([test "x$with_http" != "xno"],
        [
        # we need curl_easy_pause which was added in 7.18.0
        AC_CHECK_LIB(curl, curl_easy_pause, have_curl=yes, have_curl=no)
	], [have_curl=no])

AS_IF([test "x$have_curl" = "xyes"], [
        if test "$ac_cv_lib_curl_curl_easy_pause" != "none required"; then
                LIBWANDIO_LIBS="$LIBWANDIO_LIBS -lcurl"
        fi
        AC_DEFINE(HAVE_HTTP, 1, "Compiled with http support")
        with_http=yes],


        [AS_IF([test "x$with_http" = "xyes"],
                [AC_MSG_ERROR([http requested but libcurl (> 7.18.0) not found])])
        AC_DEFINE(HAVE_HTTP, 0, "Compiled with http support")
        with_http=no]
)

# Define automake conditionals for use in our Makefile.am files
AM_CONDITIONAL([HAVE_BZLIB], [test "x$with_bzip2" != "xno"])
AM_CONDITIONAL([HAVE_ZLIB], [test "x$with_zlib" != "xno"])
AM_CONDITIONAL([HAVE_LZO], [ test "x$with_lzo" != "xno"])
AM_CONDITIONAL([HAVE_LZMA], [ test "x$with_lzma" != "xno"])
AM_CONDITIONAL([HAVE_ZSTD], [ test "x$with_zstd" != "xno"])
AM_CONDITIONAL([HAVE_QATZIP], [ test "x$with_qatzip" != "xno"])
AM_CONDITIONAL([HAVE_LZ4], [ test "x$with_lz4" != "xno"])
AM_CONDITIONAL([HAVE_ZSTD_OR_LZ4], [ test "x$with_zstd" != "xno" -o "x$with_lz4" != "xno"])
AM_CONDITIONAL([HAVE_HTTP], [ test "x$with_http" != "xno"])

# Set all our output variables
AC_SUBST([LIBWANDIO_LIBS])
#AC_SUBST([ADD_LIBS])
AC_SUBST([ADD_INCLS])
AC_SUBST([ADD_LDFLAGS])
AC_SUBST([LTLIBOBJS])
AC_SUBST([PACKAGE_VERSION])
AC_SUBST([WANDIO_MAJOR])
AC_SUBST([WANDIO_MID])
AC_SUBST([WANDIO_MINOR])
AC_SUBST([LIBCFLAGS])
AC_SUBST([LIBCXXFLAGS])


# Finally, output our Makefiles
AC_OUTPUT

# Function for reporting whether an option was set or not
reportopt() {
	if test x"$2" = xtrue -o x"$2" = xyes; then
		AC_MSG_NOTICE([$1: Yes])
	else
		AC_MSG_NOTICE([$1: No])
	fi
}

reportlz4opt() {
        if test x"$2" = xframeapi; then
                AC_MSG_NOTICE([$1: Yes])
        else
            if test x"$2" = xstreamapi; then
                AC_MSG_NOTICE([$1: No, API is too old])
            else
                AC_MSG_NOTICE([$1: No])
            fi
        fi
}



# Report which configure options were set
echo
AC_MSG_NOTICE([WANDIO version $PACKAGE_VERSION])
reportopt "Compiled with compressed file (zlib) support" $with_zlib
reportopt "Compiled with compressed file (bz2) support" $with_bzip2
reportopt "Compiled with compressed file (lzo write only) support" $with_lzo
reportopt "Compiled with compressed file (lzma) support" $with_lzma
reportopt "Compiled with compressed file (zstd) support" $with_zstd
reportlz4opt "Compiled with compressed file (lz4) support" $with_lz4
reportopt "Compiled with Intel QuickAssist Technology support" $with_qatzip
reportopt "Compiled with http read (libcurl) support" $with_http
