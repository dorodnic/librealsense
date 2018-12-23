cmake_minimum_required(VERSION 2.8.9)

project(realsense2-gl)

set(REALSENSE_GL_CPP
    synthetic-stream-gl.h
    synthetic-stream-gl.cpp
    yuy2rgb-gl.h
    yuy2rgb-gl.cpp
    pointcloud-gl.h
    pointcloud-gl.cpp
    rs-gl.cpp
    realsense-gl.def
    ../../include/librealsense2-gl/rs_processing_gl.h
    ../../include/librealsense2-gl/rs_processing_gl.hpp
    ../../third-party/glad/glad.c
    ../../common/gl/shader.cpp
    ../../common/gl/shader.h
    ../../common/gl/vao.h
    ../../common/gl/vao.cpp
    ../../common/gl/vbo.h
    ../../common/gl/vbo.cpp
    ../../common/gl/texture-2d-shader.cpp
    ../../common/gl/texture-2d-shader.h
    ../../common/gl/pc-shader.cpp
    ../../common/gl/pc-shader.h
    ../../common/gl/loader.h
    ../../common/gl/fbo.h
    ../../common/gl/fbo.cpp
)

if(BUILD_EASYLOGGINGPP)
    list(APPEND REALSENSE_GL_CPP ../../third-party/easyloggingpp/src/easylogging++.cc)
endif()

add_library(${PROJECT_NAME} ${REALSENSE_GL_CPP})

find_package(OpenGL)
if(NOT OPENGL_FOUND)
    message(FATAL_ERROR "\n\n OpenGL package is missing!\n\n")
endif()

set(DEPENDENCIES realsense2 ${OPENGL_LIBRARIES})

if(WIN32)
    list(APPEND DEPENDENCIES glfw)
else()
    # Find glfw header
    find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h
        PATHS /usr/X11R6/include
                /usr/include/X11
                /opt/graphics/OpenGL/include
                /opt/graphics/OpenGL/contrib/libglfw
                /usr/local/include
                /usr/include/GL
                /usr/include
    )
    # Find glfw library
    find_library(GLFW_LIBRARIES NAMES glfw glfw3
        PATHS /usr/lib64
                /usr/lib
                /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
                /usr/local/lib64
                /usr/local/lib
                /usr/local/lib/${CMAKE_LIBRARY_ARCHITECTURE}
                /usr/X11R6/lib
    )
    if(APPLE)
        find_library(COCOA_LIBRARY Cocoa)
        find_library(IOKIT_LIBRARY IOKit)
        find_library(COREVIDEO_LIBRARY CoreVideo)
        LIST(APPEND DEPENDENCIES ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
    endif()
    list(APPEND DEPENDENCIES m ${GLFW_LIBRARIES})
    include_directories(${GLFW_INCLUDE_DIR})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/.."
    "${CMAKE_CURRENT_LIST_DIR}/../../include"
    "${CMAKE_CURRENT_LIST_DIR}/../../common/gl/"
    "${CMAKE_CURRENT_LIST_DIR}/../../common/"
    "${CMAKE_CURRENT_LIST_DIR}/../../third-party/glad"
    "${CMAKE_CURRENT_LIST_DIR}/../../third-party/glfw/include"
)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER Library)
target_link_libraries(${PROJECT_NAME} PRIVATE ${DEPENDENCIES})

set(CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/../cmake/realsense2")

install(TARGETS ${PROJECT_NAME}
    EXPORT realsense2Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
