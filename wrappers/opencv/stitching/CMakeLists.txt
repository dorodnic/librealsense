#  minimum required cmake version: 3.1.0
cmake_minimum_required(VERSION 3.1.0)

project(RealSenseGrabCutsExample)

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

find_package(OpenGL)
if(NOT OPENGL_FOUND)
	message(FATAL_ERROR "\n\n OpenGL package is missing!\n\n")
endif()

list(APPEND DEPENDENCIES ${OPENGL_LIBRARIES})

set(PROTOTEXT_FILE "MobileNetSSD_deploy.prototxt")
set(PROTOTEXT_URL "https://raw.githubusercontent.com/chuanqi305/MobileNet-SSD/master/MobileNetSSD_deploy.prototxt")
set(PROTOTEXT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${PROTOTEXT_FILE}")

set(MODEL_FILE "MobileNetSSD_deploy.caffemodel")
set(MODEL_URL "https://drive.google.com/uc?export=download&id=0B3gersZ2cHIxRm5PMWRoTkdHdHc")
set(MODEL_PATH "${CMAKE_CURRENT_BINARY_DIR}/${MODEL_FILE}")

if(NOT EXISTS "${PROTOTEXT_PATH}")
	message("Downloading ${PROTOTEXT_FILE} into ${CMAKE_CURRENT_BINARY_DIR}")
    file(DOWNLOAD "${PROTOTEXT_URL}" "${PROTOTEXT_PATH}")
endif()

if(NOT EXISTS "${MODEL_PATH}")
	message("Downloading ${MODEL_FILE} into ${CMAKE_CURRENT_BINARY_DIR}")
    file(DOWNLOAD "${MODEL_URL}" "${MODEL_PATH}")
endif()

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

if(WIN32)
	list(APPEND DEPENDENCIES glfw3)
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
	list(APPEND DEPENDENCIES m ${GLFW_LIBRARIES})
	include_directories(${GLFW_INCLUDE_DIR})
endif()

add_executable(rs-stitch rs-stitch.cpp ../cv-helpers.hpp 
../../../third-party/imgui/imgui.cpp 
../../../third-party/imgui/imgui_draw.cpp 
../../../third-party/imgui/imgui_impl_glfw.cpp
../../../common/ux-window.cpp
../../../common/model-views.cpp
)
target_link_libraries(rs-stitch ${DEPENDENCIES})
include_directories(rs-align ../../../common ../../../third-party/imgui ../../../third-party)

set_target_properties (rs-stitch PROPERTIES
	FOLDER "Examples/OpenCV"
)

install(
	TARGETS

	rs-stitch

	RUNTIME DESTINATION
	${CMAKE_INSTALL_PREFIX}/bin
)