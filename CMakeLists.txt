cmake_minimum_required(VERSION 3.10)
project(ncnn_opencv_server)

# 确保在 project() 之后设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENCV REQUIRED opencv4)

# 查找 nlohmann_json（用于JSON配置解析）
find_package(nlohmann_json REQUIRED)

include_directories(
    ${PROJECT_SOURCE_DIR}/lib/install_ncnn/include
    ${PROJECT_SOURCE_DIR}/lib/install_ncnn/include/ncnn
    ${PROJECT_SOURCE_DIR}/lib/install_inotify/include
    ${PROJECT_SOURCE_DIR}/lib/install_spdlog/include
    ${PROJECT_SOURCE_DIR}/lib
    ${OPENCV_INCLUDE_DIRS}
)
link_directories(
    ${PROJECT_SOURCE_DIR}/lib/install_ncnn/lib
    ${PROJECT_SOURCE_DIR}/lib/install_inotify/lib
    ${PROJECT_SOURCE_DIR}/lib/install_spdlog/lib
    ${OPENCV_LIBRARY_DIRS}
)

# 尝试在常用路径中查找 glslang 和 SPIRV 库（如不需要 Vulkan 可删）
# find_library(GLSLANG_LIBRARY glslang
#     HINTS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
#     ${PROJECT_SOURCE_DIR}/lib/install_ncnn/lib
# )
# find_library(SPIRV_LIBRARY SPIRV
#     HINTS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
#     ${PROJECT_SOURCE_DIR}/lib/install_ncnn/lib
# )

find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# 抑制特定的警告
add_compile_options(-Wno-deprecated-enum-enum-conversion)

add_executable(ncnn_opencv_server ${SRC_FILES})

# 设置可执行文件输出目录到项目根目录的 workspace 文件夹
set_target_properties(ncnn_opencv_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/workspace
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_SOURCE_DIR}/workspace
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_SOURCE_DIR}/workspace
)

# 智能链接 glslang/SPIRV（如果需要，否则忽略，不会硬报错）
set(LINK_EXTRA)
if(GLSLANG_LIBRARY AND SPIRV_LIBRARY)
    list(APPEND LINK_EXTRA ${GLSLANG_LIBRARY} ${SPIRV_LIBRARY})
else()
    message(WARNING "glslang or SPIRV not found! If ncnn is built without Vulkan, this is fine. If Vulkan is needed, please install libglslang-dev.")
endif()

target_link_libraries(ncnn_opencv_server
    ${OPENCV_LIBRARIES}
    ${LINK_EXTRA}
    ncnn
    curl
    inotify-cpp
    spdlog
    OpenMP::OpenMP_CXX
    nlohmann_json::nlohmann_json
)