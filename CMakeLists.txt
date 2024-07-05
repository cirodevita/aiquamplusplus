cmake_minimum_required(VERSION 3.18)
project(aiquamplusplus)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include the support to external projects
include(ExternalProject)
# Set the external install location
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
# Add to the includes
include_directories(SYSTEM ${EXTERNAL_INSTALL_LOCATION}/include)
# Add to the libraries
link_directories(${EXTERNAL_INSTALL_LOCATION}/lib)

# nlohmann
include(FetchContent)
FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.7.3)
FetchContent_GetProperties(json)
if(NOT json_POPULATED)
    FetchContent_Populate(json)
    add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# log4cplus
ExternalProject_Add(log4cplus
        URL https://kumisystems.dl.sourceforge.net/project/log4cplus/log4cplus-stable/2.0.5/log4cplus-2.0.5.tar.gz
        TIMEOUT 360
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_INSTALL_LOCATION} CFLAGS=-fPIC CPPFLAGS=-I${EXTERNAL_INSTALL_LOCATION}/include/ LDFLAGS=-L${EXTERNAL_INSTALL_LOCATION}/lib/
        BUILD_COMMAND make
        INSTALL_COMMAND make install
        )
set(LIBLOG4CPLUS ${INSTALL_DIR}/lib/liblog4cplus.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIBLOG4CPLUS ${EXTERNAL_INSTALL_LOCATION}/lib/liblog4cplus.dylib)
endif()

# ONNX Runtime
ExternalProject_Add(onnxruntime
    URL https://github.com/microsoft/onnxruntime/releases/download/v1.18.1/onnxruntime-linux-x64-1.18.1.tgz # TODO: change version based on architecture and if use or not use GPU
    TIMEOUT 360
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND 
        ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/onnxruntime-prefix/src/onnxruntime/include ${EXTERNAL_INSTALL_LOCATION}/include
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/onnxruntime-prefix/src/onnxruntime/lib ${EXTERNAL_INSTALL_LOCATION}/lib
)

add_executable(${PROJECT_NAME} main.cpp Config.cpp Config.hpp AiquamPlusPlus.cpp AiquammPlusPlus.hpp)

# Explicit the dependencies
add_dependencies(${PROJECT_NAME} log4cplus onnxruntime)

target_link_libraries(${PROJECT_NAME} pthread dl nlohmann_json::nlohmann_json ${EXTERNAL_INSTALL_LOCATION}/lib/liblog4cplus.so ${EXTERNAL_INSTALL_LOCATION}/lib/libonnxruntime.so)