# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#set(FW_VERSION "5.11.1.100")
#set(FW_ARTIFACTORY_DIR "https://bintray.com/api/ui/download/intel-realsense/librealsense-dev/d4xx_fw")
#set(FW_BIN d4xx_fw-${FW_VERSION}.bin)

set(FW_VERSION "5_11_1_100")
set(FW_ARTIFACTORY_DIR "https://ubit-artifactory-il.intel.com/artifactory/realsense_maven_local-il-local/librealsense/FW")
set(FW_BIN Signed_Image_UVC_${FW_VERSION}.bin)
set(FW_SHA1 261af89df41949a53909c6088131ad1dd3d184ee)

set(FW_SOURCE "Remote")

set(LRS_COMMON_DIR ${CMAKE_SOURCE_DIR}/common)
set(LRS_RESOURCES_DIR ${LRS_COMMON_DIR}/res)
set(FW_OUTPUT_FILE "${LRS_RESOURCES_DIR}/d4xx_fw.h")
set(FW_BIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/fw/${FW_BIN})

# TMP definition
set(FW_OUTPUT_FILE_TMP "${FW_BIN_PATH}.tmp")

# Convert HEX to DEC
function(from_hex HEX DEC)
    string(SUBSTRING "${HEX}" 0 -1 HEX)
    string(TOUPPER "${HEX}" HEX)
    set(_res 0)
    string(LENGTH "${HEX}" _strlen)

    while(_strlen GREATER 0)
        math(EXPR _res "${_res} * 16")
        string(SUBSTRING "${HEX}" 0 1 NIBBLE)
        string(SUBSTRING "${HEX}" 1 -1 HEX)
        if(NIBBLE STREQUAL "A")
            math(EXPR _res "${_res} + 10")
        elseif(NIBBLE STREQUAL "B")
            math(EXPR _res "${_res} + 11")
        elseif(NIBBLE STREQUAL "C")
            math(EXPR _res "${_res} + 12")
        elseif(NIBBLE STREQUAL "D")
            math(EXPR _res "${_res} + 13")
        elseif(NIBBLE STREQUAL "E")
            math(EXPR _res "${_res} + 14")
        elseif(NIBBLE STREQUAL "F")
            math(EXPR _res "${_res} + 15")
        else()
            math(EXPR _res "${_res} + ${NIBBLE}")
        endif()

        string(LENGTH "${HEX}" _strlen)
    endwhile()
    
    set(${DEC} ${_res} PARENT_SCOPE)
endfunction()

# Creates hex buffer from binary file - binary file must be 4 bytes aligned
function(readBinHeaderField input_bin_file offset limit output_hex_buffer)
        #message(STATUS "Reading BIN header from ${input_bin_file} offset ${offset} limit ${limit}")

        file(READ ${input_bin_file} buffer LIMIT ${limit} OFFSET ${offset} HEX)
        string(TOUPPER "${buffer}" buffer)
        string(REGEX REPLACE "([0-9A-F][0-9A-F])" "\\1" buffer "${buffer}")
        from_hex("${buffer}" buffer)

        # Copy ready buffer to output buffer
        set(${output_hex_buffer} "${buffer}" PARENT_SCOPE)
endfunction()

# Creates hex buffer from binary file - binary file must be 4 bytes aligned
function(bin2h input_bin_file offset output_hex_buffer)
        # message(STATUS "Creating HEX buffer from ${input_bin_file} offset ${offset}")

        # Read hex data from file
        file(READ ${input_bin_file} buffer OFFSET ${offset} HEX)

        # Move all buffer to upper case
        string(TOUPPER "${buffer}" buffer)

        # Convert every 4 bytes from AABBCCDD to 0xDDCCBBAA
        string(REGEX REPLACE "([0-9A-F][0-9A-F])" "0x\\1," buffer "${buffer}")

        # Add new line to every 16 columns
        string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\\n" buffer "${buffer}")

        # Copy ready buffer to output buffer
        set(${output_hex_buffer} "${buffer}" PARENT_SCOPE)
endfunction()


if(FW_VERSION)
    message(STATUS "--------------------------------------------------------------------------------------------------------------------------------------------------------------")

    if (FW_SOURCE MATCHES "Remote")
        file(REMOVE ${FW_BIN})
        message(STATUS "- Downloading FW ${FW_VERSION} from ${FW_ARTIFACTORY_DIR}/${FW_BIN}")
        file(DOWNLOAD "${FW_ARTIFACTORY_DIR}/${FW_BIN}" "${FW_BIN_PATH}"
            EXPECTED_HASH SHA1=${FW_SHA1}
            STATUS status)

        list(GET status 0 error_code)
        if (NOT ${error_code} EQUAL 0)
            message(FATAL_ERROR "Download firmwnare (${status}) - ${FW_ARTIFACTORY_DIR}/${FW_BIN}")
        else()
            message(STATUS "Download firmware ${status} for ${FW_BIN}")
        endif()
    endif()

    if (FW_SOURCE MATCHES "Remote")
        file(REMOVE ${FW_OUTPUT_FILE_TMP})
        message(STATUS "  Converting FW version ${FW_VERSION} from ${FW_BIN_PATH} to ${FW_OUTPUT_FILE}")

        # Create empty output file
        file(WRITE ${FW_OUTPUT_FILE_TMP} "")
        file(APPEND ${FW_OUTPUT_FILE_TMP} "/*******************************************************************************\n")
        file(APPEND ${FW_OUTPUT_FILE_TMP} "INTEL CORPORATION PROPRIETARY INFORMATION\n")
        file(APPEND ${FW_OUTPUT_FILE_TMP} "Copyright(c) 2017 Intel Corporation. All Rights Reserved.\n")
        file(APPEND ${FW_OUTPUT_FILE_TMP} "*******************************************************************************/\n\n")
        file(APPEND ${FW_OUTPUT_FILE_TMP} "#ifndef D400_LATEST_FW_H\n")
        file(APPEND ${FW_OUTPUT_FILE_TMP} "#define D400_LATEST_FW_H\n\n")
        file(APPEND ${FW_OUTPUT_FILE_TMP} "#define FW_VERSION \"${FW_VERSION}\"\n\n")
        file(APPEND ${FW_OUTPUT_FILE_TMP} "static uint8_t d400_latest_fw []  = {\n")
    
        bin2h(${FW_BIN_PATH} 0 fw_ready_buffer)

        file(APPEND ${FW_OUTPUT_FILE_TMP} "${fw_ready_buffer}\n")
        file(APPEND ${FW_OUTPUT_FILE_TMP} "};\n")
        file(APPEND ${FW_OUTPUT_FILE_TMP} "#endif\n")

        # Checking if current FW is identical to downloaded FW, and remove new one to avoid libtm recompilation    
        if(EXISTS "${FW_OUTPUT_FILE}")
            # message(STATUS "Local file ${FW_OUTPUT_FILE} already exists, comparing with downloaded FW file")
            execute_process( COMMAND ${CMAKE_COMMAND} -E compare_files "${FW_OUTPUT_FILE}" "${FW_OUTPUT_FILE_TMP}" RESULT_VARIABLE compare_result OUTPUT_QUIET ERROR_QUIET)
            if( compare_result EQUAL 0)
                message(STATUS "  File ${FW_OUTPUT_FILE} wasn't changed")
                file(REMOVE ${FW_OUTPUT_FILE_TMP})
            elseif( compare_result EQUAL 1)
                message(STATUS "  File ${FW_OUTPUT_FILE} was updated")
                file(REMOVE ${FW_OUTPUT_FILE})
                execute_process( COMMAND ${CMAKE_COMMAND} -E rename "${FW_OUTPUT_FILE_TMP}" "${FW_OUTPUT_FILE}" RESULT_VARIABLE compare_result OUTPUT_QUIET ERROR_QUIET)
            else()
                message(FATAL_ERROR "  Error while comparing the files ${FW_OUTPUT_FILE} and ${FW_OUTPUT_FILE_TMP}")
            endif()
        else()
            # message(STATUS "Rename ${FW_OUTPUT_FILE_TMP} to ${FW_OUTPUT_FILE}")
            execute_process( COMMAND ${CMAKE_COMMAND} -E rename "${FW_OUTPUT_FILE_TMP}" "${FW_OUTPUT_FILE}" RESULT_VARIABLE compare_result OUTPUT_QUIET ERROR_QUIET)
        endif()
    else()
        if (NOT EXISTS "${FW_OUTPUT_FILE}")
            message(FATAL_ERROR "  File ${FW_OUTPUT_FILE} is missing" )
        else()
            message(STATUS "  Already created ${FW_OUTPUT_FILE}")
        endif()
    endif()
endif(FW_VERSION) 