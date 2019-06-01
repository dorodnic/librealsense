# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 Intel Corporation. All Rights Reserved.
#  minimum required cmake version: 3.1.0
cmake_minimum_required(VERSION 3.1.0)

project(RealsenseToolsRealSenseViewer)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(${CMAKE_BINARY_DIR})

include(../../common/CMakeLists.txt)

if(BUILD_GRAPHICAL_EXAMPLES)
    set(RS_VIEWER_CPP
        ${COMMON_SRC}
        realsense-viewer.cpp
        ../../third-party/imgui/imgui.cpp
        ../../third-party/imgui/imgui_draw.cpp
        ../../third-party/imgui/imgui_impl_glfw.cpp
        ../../third-party/imgui/imgui-fonts-karla.hpp
        ../../third-party/imgui/imgui-fonts-fontawesome.hpp
        ../../third-party/glad/glad.c
        ../../third-party/tinyfiledialogs/tinyfiledialogs.c
        ../../third-party/tinyfiledialogs/tinyfiledialogs.h
        )

    # config-ui
    if(WIN32)
        add_executable(realsense-viewer WIN32
                ${RS_VIEWER_CPP} ${CMAKE_CURRENT_SOURCE_DIR}/res/resource.h
                ${CMAKE_CURRENT_SOURCE_DIR}/res/realsense-viewer.rc
                ../../common/windows-app-bootstrap.cpp)

        source_group("3rd Party" FILES
            ../../third-party/tinyfiledialogs/tinyfiledialogs.c
            ../../third-party/tinyfiledialogs/tinyfiledialogs.h
            ../../third-party/imgui/imgui.cpp
            ../../third-party/imgui/imgui_draw.cpp
            ../../third-party/imgui/imgui_impl_glfw.cpp
            ../../third-party/imgui/imgui-fonts-karla.hpp
            ../../third-party/imgui/imgui-fonts-fontawesome.hpp
            )

        source_group("Resources" FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/res/resource.h
                ${CMAKE_CURRENT_SOURCE_DIR}/res/realsense-viewer.rc)

        include_directories(realsense-viewer ../../third-party/imgui 
        ../../third-party/glad ../../common
        ../../third-party ${CMAKE_CURRENT_SOURCE_DIR}/res/)
    else()
        add_executable(realsense-viewer ${RS_VIEWER_CPP})
        include_directories(realsense-viewer ../../third-party/imgui 
        ../../third-party/glad ../../common ../../third-party ../../third-party/tinyfiledialogs)
    endif()
    set_property(TARGET realsense-viewer PROPERTY CXX_STANDARD 11)

    target_link_libraries(realsense-viewer ${DEPENDENCIES}
                                           ${GTK3_LIBRARIES}
                                           Threads::Threads
                                           realsense2-gl
                                           )

    set_target_properties (realsense-viewer PROPERTIES
        FOLDER Tools
    )

    if (IMPORT_UVC_FW)
        add_definitions(-DINTERNAL_FW)
        target_link_libraries(realsense-viewer uvc_fw)
    endif()

    install(
        TARGETS

        realsense-viewer

        RUNTIME DESTINATION
        ${CMAKE_INSTALL_BINDIR}
    )
endif()
