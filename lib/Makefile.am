lib_LTLIBRARIES=libwandio.la

include_HEADERS=wandio.h

AM_CFLAGS=@LIBCFLAGS@ @CFLAG_VISIBILITY@
AM_CXXFLAGS=@LIBCXXFLAGS@ @CFLAG_VISIBILITY@

if HAVE_ZLIB
LIBTRACEIO_ZLIB=ior-zlib.c iow-zlib.c
else
LIBTRACEIO_ZLIB=
endif

if HAVE_BZLIB
LIBTRACEIO_BZLIB=ior-bzip.c iow-bzip.c
else
LIBTRACEIO_BZLIB=
endif

if HAVE_LZO
LIBTRACEIO_LZO=iow-lzo.c
else
LIBTRACEIO_LZO=
endif

if HAVE_LZMA
LIBTRACEIO_LZMA=ior-lzma.c iow-lzma.c
else
LIBTRACEIO_LZMA=
endif

if HAVE_ZSTD
LIBTRACEIO_ZSTD=iow-zstd.c
else
LIBTRACEIO_ZSTD=
endif

if HAVE_LZ4
LIBTRACEIO_LZ4=iow-lz4.c
else
LIBTRACEIO_LZ4=
endif

if HAVE_ZSTD_OR_LZ4
LIBTRACEIO_ZSTD_LZ4=ior-zstd-lz4.c
else
LIBTRACEIO_ZSTD_LZ4=
endif

# if we have HTTP support, we also build Swift support
if HAVE_HTTP
LIBTRACEIO_HTTP=ior-http.c curl-helper.c curl-helper.h \
	ior-swift.c \
	swift-support/keystone.c swift-support/keystone.h \
	swift-support/jsmn.c swift-support/jsmn.h \
	swift-support/jsmn_utils.c swift-support/jsmn_utils.h
else
LIBTRACE_HTTP=
endif

if HAVE_QATZIP
LIBTRACEIO_QATZIP=ior-qat.c iow-qat.c
else
LIBTRACEIO_QATZIP=
endif

libwandio_la_SOURCES=wandio.c ior-peek.c ior-stdio.c ior-thread.c \
		iow-stdio.c iow-thread.c wandio.h wandio_internal.h \
		$(LIBTRACEIO_ZLIB) $(LIBTRACEIO_BZLIB) $(LIBTRACEIO_LZO) \
                $(LIBTRACEIO_LZMA) $(LIBTRACEIO_HTTP) $(LIBTRACEIO_ZSTD) \
                $(LIBTRACEIO_LZ4)  $(LIBTRACEIO_ZSTD_LZ4) \
                $(LIBTRACEIO_QATZIP)

AM_CPPFLAGS = @ADD_INCLS@
libwandio_la_LIBADD = @LIBWANDIO_LIBS@
libwandio_la_LDFLAGS=-version-info 6:3:0 @ADD_LDFLAGS@

