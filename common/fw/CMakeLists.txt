# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 Intel Corporation. All Rights Reserved.
cmake_minimum_required(VERSION 3.1.3)

project(uvc_fw)

set(D4XX_FW_VERSION "5.11.1.100")
set(D4XX_FW_SHA1 261af89df41949a53909c6088131ad1dd3d184ee)
set(D4XX_FW_URL "http://realsense-hw-public.s3-eu-west-1.amazonaws.com/Releases/RS4xx/FW")

set(SR3XX_FW_VERSION "3.27.3.0")
set(SR3XX_FW_SHA1 ddc6d015ee3ec46f5e5587cc5ca17d343654ce85)
set(SR3XX_FW_URL "http://realsense-hw-public.s3-eu-west-1.amazonaws.com/Releases/SR300/FW")

#set(FW_URL "https://ubit-artifactory-il.intel.com/artifactory/realsense_maven_local-il-local/librealsense/FW")
#http://realsense-hw-public.s3-eu-west-1.amazonaws.com/Releases/RS4xx/FW/D4XX_FW_Image-5.11.1.100.bin 
#http://realsense-hw-public.s3-eu-west-1.amazonaws.com/Releases/SR300/FW/SR3XX_FW_Image-3.27.3.0.bin 
add_library(${PROJECT_NAME} STATIC empty.c)

if (MSVC)
  # lib.exe can't handle multiple .res files, so include them in one.
  # even then, the linker won't grab a .res out of a .lib object, so it needs to be explicitly listed
  # and to find the name of the .res file (across cmake generators) we need to create our own rule. :(
  add_custom_command(TARGET ${PROJECT_NAME} BYPRODUCTS ${PROJECT_NAME}.res COMMAND ${CMAKE_RC_COMPILER} ${CMAKE_RC_FLAGS} /I . /fo "${PROJECT_NAME}.res" "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc")
  target_link_libraries(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.res")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER Resources)

function(target_binary url version sha1 symbol ext)
  set(binary "${CMAKE_CURRENT_BINARY_DIR}/${symbol}-${version}${ext}")
  file(DOWNLOAD "${url}/${symbol}-${version}${ext}" "${binary}"
       EXPECTED_HASH SHA1=${sha1}
       STATUS status)
  list(GET status 0 error_code)
  if (NOT ${error_code} EQUAL 0)
    message(FATAL_ERROR "Download firmwnare (${status}) - ${url}")
  else()
    message(STATUS "Download firmware ${status} for ${symbol}-${version}${ext}")
  endif()
  string(TOUPPER ${symbol} SYMBOL)
  string(REPLACE "." "," version_commas ${version})
  string(REPLACE "\\" "\\\\" binary_escaped "${binary}")
  configure_file(fw.c.in  "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.c" @ONLY)
  configure_file(fw.h.in  "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.h" @ONLY)
  configure_file(fw.rc.in "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.rc" @ONLY)
  if (MSVC)
    enable_language(RC)
    set_source_files_properties("${PROJECT_NAME}.rc" OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.rc")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/${symbol}.rc" OBJECT_DEPENDS "${binary}")
  else()
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/${symbol}.c" OBJECT_DEPENDS "${binary}")
  endif()
  target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.c")
endfunction()

target_binary( "${D4XX_FW_URL}"  "${D4XX_FW_VERSION}"  "${D4XX_FW_SHA1}" D4XX_FW_Image .bin)
target_binary( "${SR3XX_FW_URL}"  "${SR3XX_FW_VERSION}"  "${SR3XX_FW_SHA1}" SR3XX_FW_Image .bin)

install(TARGETS ${PROJECT_NAME} EXPORT realsense2Targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
