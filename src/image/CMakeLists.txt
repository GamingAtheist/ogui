cmake_minimum_required(VERSION 2.8)
include_directories(../../include)
IF (MSVC)
  add_definitions( "/wd4005 /wd4996 /nologo" )
ENDIF(MSVC)
IF(CMAKE_COMPILER_IS_GNUCXX)
  add_definitions("-std=c++11")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)
add_library(image adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c gzwrite.c infback.c inffast.c inflate.c inftrees.c png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c pngwtran.c pngwutil.c rgba_image.cpp trees.c uncompr.c zstreams.cpp zutil.c)
