cmake_minimum_required(VERSION 3.18)
project(aiquamplusplus)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_MPI "Use MPI for distributed memory parallelism." OFF)
option(USE_OMP "Use OMP for shared memory parallelism." OFF)
option(USE_CUDA "Use CUDA acceleration." OFF)

set(LIBMPI "")
find_package(MPI)
if(MPI_C_FOUND)
    if (USE_MPI)
        message(STATUS "Using MPI for distributed memory parallelism.")
        add_definitions(-DUSE_MPI)
        include_directories(${MPI_INCLUDE_PATH})
        set(LIBMPI ${MPI_C_LIBRARIES})
    endif()
endif()

set(LIBOMP "")
find_package(OpenMP)
if(OpenMP_CXX_FOUND AND USE_OMP)
    add_definitions(-DUSE_OMP)
    message(STATUS "Using OMP for shared memory parallelism.")
    set(LIBOMP OpenMP::OpenMP_CXX)
endif()

find_package(CUDA)
if(CUDA_FOUND AND USE_CUDA)
    message(STATUS "Use CUDA acceleration.")
    message(STATUS "CUDA Version: ${CUDA_VERSION_STRING}")
    enable_language("CUDA")
    if(${CUDA_VERSION_STRING} VERSION_GREATER_EQUAL "11.1")
      		execute_process(COMMAND ${CMAKE_CUDA_COMPILER} --list-gpu-code RESULT_VARIABLE EXIT_CODE OUTPUT_VARIABLE OUTPUT_VAL)
      		if(EXIT_CODE EQUAL 0)
        		#Remove sm_
        		string(REPLACE "sm_" "" OUTPUT_VAL ${OUTPUT_VAL})
        		#Convert to list
        		string(REPLACE "\n" ";" __CUDA_ARCH_BIN ${OUTPUT_VAL})
        		#Remove last empty entry
        		list(REMOVE_AT __CUDA_ARCH_BIN -1)
      		else()
        		message(FATAL_ERROR "Failed to run NVCC to get list of GPU codes: ${EXIT_CODE}")
      		endif()
    	elseif(${CUDA_VERSION_STRING} VERSION_GREATER_EQUAL "11.0")
      		set(__CUDA_ARCH_BIN "35;37;50;52;53;60;61;62;70;72;75;80")
    	elseif(${CUDA_VERSION_STRING} VERSION_GREATER_EQUAL "10.0")
      		set(__CUDA_ARCH_BIN "30;32;35;37;50;52;53;60;61;62;70;72;75")
    	elseif(${CUDA_VERSION_STRING} VERSION_GREATER_EQUAL "9.1")
      		set(__CUDA_ARCH_BIN "30;32;35;37;50;52;53;60;61;62;70;72")
    	else()
      		set(__CUDA_ARCH_BIN "30;32;35;37;50;52;53;60;61;62;70")
    	endif()
	add_definitions(-DUSE_CUDA)
    
	set(CUDA_ARCH_BIN ${__CUDA_ARCH_BIN} CACHE STRING "Specify 'real' GPU architectures to build binaries for")
    	if(POLICY CMP0104)
		set(CMAKE_CUDA_ARCHITECTURES ${CUDA_ARCH_BIN})
    		message(STATUS "CUDA Architectures: ${CMAKE_CUDA_ARCHITECTURES}")
	else()
		# Generate SASS
        set(CMAKE_CUDA_ARCHITECTURES ${CUDA_ARCH_BIN})
        # Generate PTX for last architecture
        list(GET CUDA_ARCH_BIN -1 ver)
        set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_${ver},code=compute_${ver}")
        message(STATUS "CMAKE_CUDA_FLAGS: ${CMAKE_CUDA_FLAGS}")

        add_library(pcl_cuda INTERFACE)
        target_include_directories(pcl_cuda INTERFACE ${CUDA_TOOLKIT_INCLUDE})
	endif()

    include_directories(${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
    find_library(CUDART_LIBRARY cudart ${CMAKE_CUDA_IMPLICIT_LINK_DIRECTORIES})
endif()

# Include the support to external projects
include(ExternalProject)
# Set the external install location
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
# Add to the includes
include_directories(SYSTEM ${EXTERNAL_INSTALL_LOCATION}/include)
# Add to the libraries
link_directories(${EXTERNAL_INSTALL_LOCATION}/lib)

set(NETCDFCXXLIBS_PARAMS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(NETCDFCXXLIBS_PARAMS "-lnetcdf -lhdf5_hl -lhdf5 -lm -lz -lcurl")
endif()

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
set(LIBLOG4CPLUS ${EXTERNAL_INSTALL_LOCATION}/lib/liblog4cplus.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIBLOG4CPLUS ${EXTERNAL_INSTALL_LOCATION}/lib/liblog4cplus.dylib)
endif()

# ONNX Runtime
set(ONNXRUNTIME_URL_BASE "https://github.com/microsoft/onnxruntime/releases/download")
set(ONNXRUNTIME_VERSION "1.18.1" CACHE STRING "Version of ONNX Runtime to use")
set(ONNXRUNTIME_CUDA_VERSION "12" CACHE STRING "CUDA version to use with ONNX Runtime")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ONNXRUNTIME_PLATFORM "linux-x64")
    if(CUDA_FOUND AND USE_CUDA)
        set(ONNXRUNTIME_PLATFORM "${ONNXRUNTIME_PLATFORM}-gpu-cuda${ONNXRUNTIME_CUDA_VERSION}")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(ONNXRUNTIME_PLATFORM "osx-arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(ONNXRUNTIME_PLATFORM "osx-x86_64")
    else()
        set(ONNXRUNTIME_PLATFORM "osx-universal2")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(ONNXRUNTIME_PLATFORM "linux-aarch64")
endif()
ExternalProject_Add(onnxruntime
    URL ${ONNXRUNTIME_URL_BASE}/v${ONNXRUNTIME_VERSION}/onnxruntime-${ONNXRUNTIME_PLATFORM}-${ONNXRUNTIME_VERSION}.tgz
    TIMEOUT 360
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND 
        ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/onnxruntime-prefix/src/onnxruntime/include ${EXTERNAL_INSTALL_LOCATION}/include
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/onnxruntime-prefix/src/onnxruntime/lib ${EXTERNAL_INSTALL_LOCATION}/lib
)
set(ONNXRUNTIME ${EXTERNAL_INSTALL_LOCATION}/lib/libonnxruntime.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(ONNXRUNTIME ${EXTERNAL_INSTALL_LOCATION}/lib/libonnxruntime.dylib)
endif()

# szlib
ExternalProject_Add(szlib
    URL https://support.hdfgroup.org/ftp/lib-external/szip/2.1.1/src/szip-2.1.1.tar.gz
    TIMEOUT 360
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure CFLAGS=-fPIC CPPFLAGS=-I${EXTERNAL_INSTALL_LOCATION}/include/ LDFLAGS=-L${EXTERNAL_INSTALL_LOCATION}/lib/ --prefix=${EXTERNAL_INSTALL_LOCATION}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)
set(LIBSZLIB ${EXTERNAL_INSTALL_LOCATION}/lib/libsz.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIBSZLIB ${EXTERNAL_INSTALL_LOCATION}/lib/libsz.dylib)
endif()

# zlib
ExternalProject_Add(zlib
    URL https://zlib.net/zlib-1.3.1.tar.gz
    TIMEOUT 360
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND CFLAGS=-fPIC ./configure --prefix=${EXTERNAL_INSTALL_LOCATION}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)
set(LIBZLIB ${EXTERNAL_INSTALL_LOCATION}/lib/libz.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIBZLIB ${EXTERNAL_INSTALL_LOCATION}/lib/libz.dylib)
endif()

# curl
ExternalProject_Add(curl
    URL https://curl.se/download/curl-8.7.1.tar.gz
    TIMEOUT 360
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_INSTALL_LOCATION} CFLAGS=-fPIC CPPFLAGS=-I${EXTERNAL_INSTALL_LOCATION}/include/ LDFLAGS=-L${EXTERNAL_INSTALL_LOCATION}/lib/ --disable-ldap --without-brotli --without-ssl
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)
set(LIBCURL ${EXTERNAL_INSTALL_LOCATION}/lib/libcurl.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIBCURL ${EXTERNAL_INSTALL_LOCATION}/lib/libcurl.dylib)
endif()

# hdf5
ExternalProject_Add(hdf5
    URL https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.12/hdf5-1.12.0/src/hdf5-1.12.0.tar.gz
    TIMEOUT 360
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_INSTALL_LOCATION} CFLAGS=-fPIC CPPFLAGS=-I${EXTERNAL_INSTALL_LOCATION}/include/ LDFLAGS=-L${EXTERNAL_INSTALL_LOCATION}/lib/ --enable-hl --enable-shared --enable-build-mode=production --enable-unsupported --enable-cxx --with-zlib=${EXTERNAL_INSTALL_LOCATION} --with-szlib=${EXTERNAL_INSTALL_LOCATION} --enable-threadsafe --with-pthread
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)
set(LIBHDF5 ${EXTERNAL_INSTALL_LOCATION}/lib/libhdf5_hl.so;${EXTERNAL_INSTALL_LOCATION}/lib/libhdf5.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIBHDF5 ${EXTERNAL_INSTALL_LOCATION}/lib/libhdf5.dylib)
endif()

# netcdf
ExternalProject_Add(netcdf
    URL https://downloads.unidata.ucar.edu/netcdf-c/4.8.1/netcdf-c-4.8.1.tar.gz
    TIMEOUT 360
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure CFLAGS=-fPIC CPPFLAGS=-I${EXTERNAL_INSTALL_LOCATION}/include/ LDFLAGS=-L${EXTERNAL_INSTALL_LOCATION}/lib/ --prefix=${EXTERNAL_INSTALL_LOCATION}  --enable-shared --enable-netcdf-4 --enable-dap --enable-byterange --enable-erange-fill
    INSTALL_COMMAND make install
)
set(LIBNETCDF ${EXTERNAL_INSTALL_LOCATION}/lib/libnetcdf.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIBNETCDF ${EXTERNAL_INSTALL_LOCATION}/lib/libnetcdf.dylib)
endif()

# netcdfcxx
ExternalProject_Add(netcdfcxx
    GIT_REPOSITORY https://github.com/Unidata/netcdf-cxx4.git
    TIMEOUT 360
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> autoreconf -i && ./configure LIBS=${NETCDFCXXLIBS_PARAMS} CFLAGS=-fPIC CPPFLAGS=-I${EXTERNAL_INSTALL_LOCATION}/include/ LDFLAGS=-L${EXTERNAL_INSTALL_LOCATION}/lib/ --prefix=${EXTERNAL_INSTALL_LOCATION} --enable-shared
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    UPDATE_COMMAND ""
)
set(LIBNETCDFCXX ${EXTERNAL_INSTALL_LOCATION}/lib/libnetcdf_c++4.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIBNETCDFCXX ${EXTERNAL_INSTALL_LOCATION}/lib/libnetcdf_c++4.dylib)
endif()

# shapelib
ExternalProject_Add(shapelib
    URL http://download.osgeo.org/shapelib/shapelib-1.6.0.tar.gz
    TIMEOUT 360
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_INSTALL_LOCATION} CFLAGS=-fPIC CPPFLAGS=-I${EXTERNAL_INSTALL_LOCATION}/include/ LDFLAGS=-L${EXTERNAL_INSTALL_LOCATION}/lib/
    INSTALL_COMMAND make install
)
set(LIBSHP ${EXTERNAL_INSTALL_LOCATION}/lib/libshp.so)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LIBSHP ${EXTERNAL_INSTALL_LOCATION}/lib/libshp.dylib)
endif()

add_executable(${PROJECT_NAME} main.cpp Array.h Config.cpp Config.hpp AiquamPlusPlus.cpp AiquamPlusPlus.hpp WacommAdapter.cpp WacommAdapter.hpp Aiquam.cpp Aiquam.hpp Areas.cpp Areas.hpp Area.cpp Area.hpp)

# Explicit the dependencies
add_dependencies(zlib szlib)
add_dependencies(curl zlib)
add_dependencies(hdf5 curl)
add_dependencies(netcdf hdf5)
add_dependencies(netcdfcxx netcdf)
add_dependencies(${PROJECT_NAME} log4cplus onnxruntime netcdfcxx shapelib)

target_link_libraries(${PROJECT_NAME} ${LIBOMP} ${LIBMPI} ${CUDART_LIBRARY} nlohmann_json::nlohmann_json ${LIBLOG4CPLUS} ${ONNXRUNTIME} ${LIBHDF5} ${LIBNETCDF} ${LIBNETCDFCXX} ${LIBSHP})