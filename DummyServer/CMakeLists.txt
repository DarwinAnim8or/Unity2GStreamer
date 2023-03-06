cmake_minimum_required(VERSION 3.0)

project(DummyServer)

# Add the source files
set(SOURCES
    CCTVServer.cpp
    CCTVServer.h
    main.cpp
)

# Add the RakNet include directory
include_directories(../3rdparty/RakNet/Source)

# Add the RakNet static library
link_directories(../3rdparty/RakNet/build/Lib/LibStatic/)
link_libraries(RakNetLibStatic)

# Create the executable
add_executable(DummyServer ${SOURCES})
