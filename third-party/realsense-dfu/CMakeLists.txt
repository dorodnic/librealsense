cmake_minimum_required(VERSION 2.8.9)

project(realsense-dfu)


FILE(GLOB_RECURSE AllSources
        ${CMAKE_CURRENT_LIST_DIR}/*.h
        ${CMAKE_CURRENT_LIST_DIR}/*.cpp
        )

add_library(${PROJECT_NAME} STATIC
    ${AllSources}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${LIBUSB_LOCAL_INCLUDE_PATH}
        )

IF(WIN32)
    include_directories(${LIBUSB_LOCAL_INCLUDE_PATH}/libusb)
ENDIF(WIN32)

target_link_libraries(${PROJECT_NAME}
    ${LIBUSB1_LIBRARIES}
)

set_target_properties (${PROJECT_NAME} PROPERTIES FOLDER Library)

set(CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/../cmake/realsense2")

install(TARGETS ${PROJECT_NAME}
    EXPORT realsense2Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
