#  minimum required cmake version: 3.1.0
cmake_minimum_required(VERSION 3.1.0)

project(RealsenseToolsRealSenseViewer)

# Save the command line compile commands in the build output
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

if(BUILD_GRAPHICAL_EXAMPLES)

if(APPLE)
    add_definitions(-DNOC_FILE_DIALOG_OSX)
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

    include_directories(${GTK3_INCLUDE_DIRS})
    link_directories(${GTK3_LIBRARY_DIRS})
    add_definitions(${GTK3_CFLAGS_OTHER})

    add_definitions(-DNOC_FILE_DIALOG_GTK)
elseif(WIN32)
    add_definitions(-DNOC_FILE_DIALOG_WIN32)
endif()

    set(RS_VIEWER_CPP
            realsense-viewer.cpp
            ../../third-party/imgui/imgui.cpp
            ../../third-party/imgui/imgui_draw.cpp
            ../../third-party/imgui/imgui_impl_glfw.cpp
            ../../third-party/imgui/imgui-fonts-karla.hpp
            ../../third-party/imgui/imgui-fonts-fontawesome.hpp
            ../../third-party/glad/glad.c
            ../../common/rendering.h
            ../../common/model-views.h
            ../../common/model-views.cpp
            ../../common/ux-window.h
            ../../common/ux-window.cpp
            ../../common/ux-alignment.cpp
            ../../common/ux-alignment.h
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
            ../../common/gl/camera-shader.h
            ../../common/gl/camera-shader.cpp
            ../../common/rs-config.h
            ../../common/rs-config.cpp
            ${LZ4_DIR}/lz4.h
            ${LZ4_DIR}/lz4.c
        )

    # config-ui
    if(WIN32)
        add_executable(realsense-viewer WIN32
                ${RS_VIEWER_CPP} ${CMAKE_CURRENT_SOURCE_DIR}/res/resource.h
                ${CMAKE_CURRENT_SOURCE_DIR}/res/realsense-viewer.rc
                ../../common/windows-app-bootstrap.cpp)

        source_group("3rd Party" FILES
            ../../third-party/imgui/imgui.cpp
            ../../third-party/imgui/imgui_draw.cpp
            ../../third-party/imgui/imgui_impl_glfw.cpp
            ../../third-party/imgui/imgui-fonts-karla.hpp
            ../../third-party/imgui/imgui-fonts-fontawesome.hpp)

        source_group("Resources" FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/res/resource.h
                ${CMAKE_CURRENT_SOURCE_DIR}/res/realsense-viewer.rc)

        include_directories(realsense-viewer ../../third-party/imgui 
        ../../third-party/glad ../../common/gl ../../common ${LZ4_DIR}
        ../../third-party ${CMAKE_CURRENT_SOURCE_DIR}/res/)
    else()
        add_executable(realsense-viewer ${RS_VIEWER_CPP})
        include_directories(realsense-viewer ../../third-party/imgui 
        ../../third-party/glad ../../common/gl ${LZ4_DIR}
        ../../common ../../third-party)
    endif()

    target_link_libraries(realsense-viewer ${DEPENDENCIES} ${GTK3_LIBRARIES})

    set_target_properties (realsense-viewer PROPERTIES
        FOLDER Tools
    )

    install(
        TARGETS

        realsense-viewer

        RUNTIME DESTINATION
        ${CMAKE_INSTALL_PREFIX}/bin
    )
endif()
