option(ENABLE_CCACHE "Build with ccache." ON)
option(BUILD_WITH_CUDA "Enable CUDA" OFF)
option(BUILD_GRAPHICAL_EXAMPLES "Build graphical examples and tools." ON)
option(BUILD_WITH_OPENMP "Use OpenMP" OFF)
option(ENABLE_ZERO_COPY "Enable zero copy functionality" OFF)
option(BUILD_WITH_TM2 "Build with support for Intel TM2 tracking device" ON)
option(BUILD_EASYLOGGINGPP "Build EasyLogging++ as a part of the build" ON)
option(BUILD_WITH_STATIC_CRT "Build with static link CRT" ON)
option(FORCE_LIBUVC "Explicitly turn-on libuvc backend" OFF)
option(FORCE_WINUSB_UVC "Explicitly turn-on winusb_uvc (for win7) backend" OFF)
option(TRACE_API "Log all C API calls" OFF)
option(HWM_OVER_XU "Send HWM commands over UVC XU control" ON)
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_UNIT_TESTS "Build realsense unit tests. Note that when enabled, additional tests data set will be downloaded from a web server and stored in a temp directory" OFF)
option(BUILD_INTERNAL_UNIT_TESTS "Test package for components under librealsense namespace, requires BUILD_SHARED_LIBS=OFF and BUILD_UNIT_TESTS=ON" OFF)
option(BUILD_EXAMPLES "Build realsense examples and tools." ON)
option(ENFORCE_METADATA "Require WinSDK with Metadata support during compilation. Windows OS Only" OFF)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
option(BUILD_PYTHON_DOCS "Build Documentation for Python bindings" OFF)
option(BUILD_CSHARP_BINDINGS "Build C# bindings" OFF)
option(BUILD_MATLAB_BINDINGS "Build Matlab bindings" OFF)
option(BUILD_UNITY_BINDINGS "Copy the unity project to the build folder with the required dependencies" OFF)
option(BUILD_CV_EXAMPLES "Build OpenCV examples" OFF)
option(BUILD_PCL_EXAMPLES "Build PCL examples" OFF)
option(BUILD_NODEJS_BINDINGS "Build Node.js bindings" OFF)
option(BUILD_OPENNI2_BINDINGS "Build OpenNI bindings" OFF)
option(ANDROID_USB_HOST_UVC "Build UVC backend for Android" OFF)
option(IMPORT_UVC_FW "Download the latest FW for the depth cameras" ON)
