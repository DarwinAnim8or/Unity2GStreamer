cmake_minimum_required(VERSION 3.0)

project(U2G_Client)

# Add the source files
set(SOURCES
    U2G_Client.cpp
)

# Find gstreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-rtsp-1.0)

#Link GStreamer:
include_directories(${GST_INCLUDE_DIRS})
link_directories(${GST_LIBRARY_DIRS})

# Add the RakNet include directory
include_directories(../3rdparty/RakNet/Source)

# Add the RakNet static library
link_directories(../3rdparty/RakNet/build/Lib/LibStatic/)
link_libraries(RakNetLibStatic)

# Create the executable
add_executable(U2G_Client ${SOURCES})
target_link_libraries(U2G_Client ${GST_LIBRARIES})
