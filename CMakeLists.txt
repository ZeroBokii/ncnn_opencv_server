cmake_minimum_required(VERSION 3.10)
project(ncnn_opencv_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED LIB_ARCH)
    if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(LIB_ARCH "arm")
    else()
        set(LIB_ARCH "amd")
    endif()
endif()

if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    message(STATUS "=== Cross-compiling for aarch64 ===")
    message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
    message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER}")
    set(IS_AARCH64_CROSS TRUE)
else()
    message(STATUS "=== Native build for x86_64 ===")
    set(IS_AARCH64_CROSS FALSE)
endif()

message(STATUS "Library architecture: ${LIB_ARCH}")

set(LIB_BASE_DIR ${PROJECT_SOURCE_DIR}/lib/${LIB_ARCH})

include_directories(
    ${LIB_BASE_DIR}/install_ncnn/include
    ${LIB_BASE_DIR}/install_ncnn/include/ncnn
    ${LIB_BASE_DIR}/install_opencv/include
    ${LIB_BASE_DIR}/install_opencv/include/opencv4
    ${LIB_BASE_DIR}/install_inotify/include
    ${LIB_BASE_DIR}/install_spdlog/include
    ${LIB_BASE_DIR}  # httplib.h
)

if(IS_AARCH64_CROSS)
    set(BOOST_INCLUDEDIR "/usr/include")
    find_package(Boost QUIET)
    if(NOT Boost_FOUND)
        message(STATUS "Boost: Using host system headers at /usr/include")
        include_directories(/usr/include)
    else()
        message(STATUS "Boost found: ${Boost_VERSION}")
        include_directories(${Boost_INCLUDE_DIRS})
    endif()
else()
    find_package(Boost REQUIRED)
    if(Boost_FOUND)
        message(STATUS "Boost found: ${Boost_VERSION}")
        message(STATUS "Boost include: ${Boost_INCLUDE_DIRS}")
        include_directories(${Boost_INCLUDE_DIRS})
    endif()
endif()

link_directories(
    ${LIB_BASE_DIR}/install_ncnn/lib
    ${LIB_BASE_DIR}/install_opencv/lib
    ${LIB_BASE_DIR}/install_inotify/lib
    ${LIB_BASE_DIR}/install_spdlog/lib
)

if(IS_AARCH64_CROSS)
    # aarch64-linux-gnu-gcc 自带 OpenMP 支持
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
    set(OPENMP_LINK_FLAGS "-fopenmp")
    message(STATUS "OpenMP: Using manual flags for cross-compilation")
else()
    find_package(OpenMP REQUIRED)
    if(OpenMP_CXX_FOUND)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    endif()
endif()

if(IS_AARCH64_CROSS)
    find_package(nlohmann_json QUIET)
    if(NOT nlohmann_json_FOUND)
        message(STATUS "nlohmann_json: Not found via find_package, assuming header-only in system path")
    endif()
else()
    find_package(nlohmann_json REQUIRED)
endif()

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_compile_options(
    -Wno-deprecated-enum-enum-conversion
    -Wno-nonnull 
)

add_executable(ncnn_opencv_server ${SRC_FILES})

set_target_properties(ncnn_opencv_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/workspace
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}/workspace
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}/workspace
)

set(LINK_EXTRA)
if(GLSLANG_LIBRARY AND SPIRV_LIBRARY)
    list(APPEND LINK_EXTRA ${GLSLANG_LIBRARY} ${SPIRV_LIBRARY})
else()
    message(WARNING "glslang or SPIRV not found! If ncnn is built without Vulkan, this is fine.")
endif()

if(IS_AARCH64_CROSS)
    target_link_libraries(ncnn_opencv_server
        ${LINK_EXTRA}
        ncnn
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        inotify-cpp
        spdlog
        mosquitto
        pthread
        dl
    )
    target_link_options(ncnn_opencv_server PRIVATE ${OPENMP_LINK_FLAGS})
    
    if(nlohmann_json_FOUND)
        target_link_libraries(ncnn_opencv_server nlohmann_json::nlohmann_json)
    endif()
else()
    target_link_libraries(ncnn_opencv_server
        ${LINK_EXTRA}
        ncnn
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        inotify-cpp
        spdlog
        mosquitto
        OpenMP::OpenMP_CXX
        nlohmann_json::nlohmann_json
    )
endif()

message(STATUS "=== Build Configuration ===")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Cross Compiling: ${CMAKE_CROSSCOMPILING}")
message(STATUS "Target Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Library Path: ${LIB_BASE_DIR}")