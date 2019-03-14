cmake_minimum_required (VERSION 3.10)
project (blackbox_tools VERSION 0.5.0)

set (COMMON_SRC	parser.c tools.c platform.c stream.c decoders.c units.c blackbox_fielddefs.c)
set (DECODER_SRC ${COMMON_SRC} blackbox_decode.c gpxwriter.c kmlwriter.c imu.c battery.c stats.c)
set (RENDERER_SRC ${COMMON_SRC} blackbox_render.c datapoints.c embeddedfont.c expo.c imu.c)
set (ENCODER_TESTBED_SRC ${COMMON_SRC} encoder_testbed.c encoder_testbed_io.c)

set (COMMON_HEADER	parser.h tools.h platform.h stream.h decoders.h units.h blackbox_fielddefs.h)
set (DECODER_HEADER ${COMMON_HEADER} gpxwriter.h kmlwriter.h imu.h battery.h stats.h)
set (RENDERER_HEADER ${COMMON_HEADER} datapoints.h embeddedfont.h expo.h imu.h)
set (ENCODER_TESTBED_HEADER ${COMMON_HEADER} encoder_testbed_io.h)

if (WIN32)
    set (DECODER_SRC ${DECODER_SRC} ../lib/getopt_mb_uni/getopt.c ../lib/getopt_mb_uni/getopt.h)
    set (RENDERER_SRC ${RENDERER_SRC} ../lib/getopt_mb_uni/getopt.c ../lib/getopt_mb_uni/getopt.h)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    link_directories(../lib/win32)
endif(WIN32)

add_definitions(-DBLACKBOX_VERSION=${PROJECT_VERSION})

if (UNIX)
    add_definitions(-D_strdup=strdup)
endif (UNIX)

add_executable (blackbox_decode ${DECODER_HEADER} ${DECODER_SRC})
if (WIN32)
    target_include_directories(blackbox_decode PRIVATE ../lib/getopt_mb_uni)
endif(WIN32)
if (UNIX)
    target_link_libraries(blackbox_decode m pthread)
endif (UNIX)

add_executable (blackbox_render  ${RENDERER_HEADER} ${RENDERER_SRC})
if (WIN32)
    target_include_directories(blackbox_render PRIVATE ../lib/cairo-1.14/src)
    target_include_directories(blackbox_render PRIVATE ../lib/freetype-2.5.3/include)
    target_include_directories(blackbox_render PRIVATE ../lib/getopt_mb_uni)
    target_link_libraries(blackbox_render cairo libfreetype-6)
endif(WIN32)
if (UNIX)
   find_package(PkgConfig REQUIRED)

   pkg_check_modules(CAIRO REQUIRED cairo)
   target_link_libraries(blackbox_render ${CAIRO_LIBRARIES})
   target_include_directories(blackbox_render PUBLIC ${CAIRO_INCLUDE_DIRS})
   target_compile_options(blackbox_render PUBLIC ${CAIRO_CFLAGS_OTHER})

   pkg_check_modules(FREETYPE2 REQUIRED freetype2)
   target_link_libraries(blackbox_render ${FREETYPE2_LIBRARIES})
   target_include_directories(blackbox_render PUBLIC ${FREETYPE2_INCLUDE_DIRS})
   target_compile_options(blackbox_render PUBLIC ${FREETYPE2_CFLAGS_OTHER})

   target_link_libraries(blackbox_render m pthread)
endif (UNIX)

# add_executable (encoder_testbed ${ENCODER_TESTBED_HEADER} ${ENCODER_TESTBED_SRC})
# target_include_directories(encoder_testbed PRIVATE ../lib/getopt_mb_uni)
