#  minimum required cmake version: 3.1.0
cmake_minimum_required(VERSION 3.1.0)

project(RealsenseToolsConvert)

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

# terminal
add_executable(rs-convert rs-convert.cpp)
if(WIN32 OR ANDROID)
    target_link_libraries(rs-convert ${DEPENDENCIES})
else()
    target_link_libraries(rs-convert -lpthread ${DEPENDENCIES})
endif()
include_directories(rs-convert ../../common ../../third-party ../../third-party/tclap/include)
set_target_properties (rs-convert PROPERTIES
    FOLDER Tools
)

install(
    TARGETS

    rs-convert

    RUNTIME DESTINATION
    ${CMAKE_INSTALL_PREFIX}/bin
)

