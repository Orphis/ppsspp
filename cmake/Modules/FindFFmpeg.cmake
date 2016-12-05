# FindFFMPEG
# ----------
#
# Find the native FFMPEG includes and libraries
#
# This module defines:
#
#  FFMPEG_INCLUDE_DIR, where to find avformat.h, avcodec.h...
#  FFMPEG_LIBRARIES, the libraries to link against to use FFMPEG.
#  FFMPEG_FOUND, If false, do not try to use FFMPEG.
#
# also defined, but not for general use are:
#
#   FFMPEG_avformat_LIBRARY, where to find the FFMPEG avformat library.
#   FFMPEG_avcodec_LIBRARY, where to find the FFMPEG avcodec library.
#
# This is useful to do it this way so that we can always add more libraries
# if needed to ``FFMPEG_LIBRARIES`` if ffmpeg ever changes...

#=============================================================================
# Copyright: 1993-2008 Ken Martin, Will Schroeder, Bill Lorensen
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of ppsspp, substitute the full
#  License text for the above reference.)

set(ALL_COMPONENTS
  avcodec
  avdevice
  avfilter
  avformat
  postproc
  swresample
  swscale
)

set(DEPS_avcodec avutil)
set(DEPS_avdevice avcodec avformat avutil)
set(DEPS_avfilter avutil)
set(DEPS_avformat avcodec avutil)
set(DEPS_postproc avutil)
set(DEPS_swresample avutil)
set(DEPS_swscale avutil)

function(find_ffmpeg LIBNAME)
  if(DEFINED ENV{FFMPEG_DIR})
    set(FFMPEG_DIR $ENV{FFMPEG_DIR})
  endif()

  if(FFMPEG_DIR)
    list(APPEND INCLUDE_PATHS
      ${FFMPEG_DIR}
      ${FFMPEG_DIR}/ffmpeg
      ${FFMPEG_DIR}/lib${LIBNAME}
      ${FFMPEG_DIR}/include/lib${LIBNAME}
      ${FFMPEG_DIR}/include/ffmpeg
      ${FFMPEG_DIR}/include
      NO_DEFAULT_PATH
    )
    list(APPEND LIB_PATHS
      ${FFMPEG_DIR}
      ${FFMPEG_DIR}/lib
      ${FFMPEG_DIR}/lib${LIBNAME}
      NO_DEFAULT_PATH
    )
  else()
    list(APPEND INCLUDE_PATHS
      /usr/local/include/ffmpeg
      /usr/local/include/lib${LIBNAME}
      /usr/include/ffmpeg
      /usr/include/lib${LIBNAME}
      /usr/include/ffmpeg/lib${LIBNAME}
    )

    list(APPEND LIB_PATHS
      /usr/local/lib
      /usr/lib
    )
  endif()


  find_path(FFMPEG_INCLUDE_${LIBNAME} lib${LIBNAME}/${LIBNAME}.h
    HINTS ${INCLUDE_PATHS}
  )

  find_library(FFMPEG_LIBRARY_${LIBNAME} ${LIBNAME}
    HINTS ${LIB_PATHS}
  )

  if(NOT FFMPEG_LIBRARY_${LIBNAME} OR NOT FFMPEG_INCLUDE_${LIBNAME})
    # Didn't find it in the usual paths, try pkg-config
    find_package(PkgConfig QUIET)
    pkg_check_modules(FFMPEG_PKGCONFIG_${LIBNAME} REQUIRED QUIET lib${LIBNAME})

    find_path(FFMPEG_INCLUDE_${LIBNAME} lib${LIBNAME}/${LIBNAME}.h
      ${FFMPEG_PKGCONFIG_${LIBNAME}_INCLUDE_DIRS}
    )

    find_library(FFMPEG_LIBRARY_${LIBNAME} ${LIBNAME}
      ${FFMPEG_PKGCONFIG_${LIBNAME}_LIBRARY_DIRS}
    )
  endif()

  set(FFMPEG_INCLUDE_${LIBNAME} "${FFMPEG_INCLUDE_${LIBNAME}}" PARENT_SCOPE)
  set(FFMPEG_LIBRARY_${LIBNAME} "${FFMPEG_LIBRARY_${LIBNAME}}" PARENT_SCOPE)
  message("--  ${LIBNAME}: ${FFMPEG_INCLUDE_${LIBNAME}} ${FFMPEG_LIBRARY_${LIBNAME}}")
endfunction()

if(NOT FFmpeg_FIND_COMPONENTS)

endif()


find_ffmpeg(avcodec)
find_ffmpeg(avformat)
find_ffmpeg(avutil)
find_ffmpeg(swresample)
find_ffmpeg(swscale)

if(FFMPEG_INCLUDE_DIR AND
    FFMPEG_avformat_LIBRARY AND
    FFMPEG_avcodec_LIBRARY AND
    FFMPEG_avutil_LIBRARY AND
    FFMPEG_swresample_LIBRARY AND
    FFMPEG_swscale_LIBRARY)
  set(FFMPEG_FOUND "YES")
  set(FFMPEG_LIBRARIES ${FFMPEG_avformat_LIBRARY}
                       ${FFMPEG_avcodec_LIBRARY}
                       ${FFMPEG_avutil_LIBRARY}
                       ${FFMPEG_swresample_LIBRARY}
                       ${FFMPEG_swscale_LIBRARY}
  )

  message("====== Defining imported targets for FFmpeg")
  add_library(FFmpeg::avcodec IMPORTED UNKNOWN)
  set_target_properties(FFmpeg::avcodec PROPERTIES
    IMPORTED_LOCATION ${FFMPEG_avcodec_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INCLUDE_AVCODEC}
  )
  target_link_libraries(FFmpeg::avcodec PUBLIC FFmpeg::avutil FFmpeg::swresample)

  add_library(FFmpeg::avformat IMPORTED UNKNOWN)
  set_target_properties(FFmpeg::avformat PROPERTIES
    IMPORTED_LOCATION ${FFMPEG_avformat_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INCLUDE_AVFORMAT}
  )
  target_link_libraries(FFmpeg::avformat PUBLIC FFmpeg::avcodec FFmpeg::avutil FFmpeg::swresample)

  set_target_properties(FFmpeg::avutil PROPERTIES
    IMPORTED_LOCATION ${FFMPEG_avutil_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INCLUDE_AVUTIL}
  )

  set_target_properties(FFmpeg::swresample PROPERTIES
    IMPORTED_LOCATION ${FFMPEG_swresample_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INCLUDE_SWRESAMPLE}
  )
  target_link_libraries(FFmpeg::swresample PUBLIC FFmpeg::avutil)
   
  set_target_properties(FFmpeg::swscale PROPERTIES
    IMPORTED_LOCATION ${FFMPEG_swscale_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INCLUDE_SWSCALE}
  )
  target_link_libraries(FFmpeg::swscale PUBLIC FFmpeg::avutil)

endif()
