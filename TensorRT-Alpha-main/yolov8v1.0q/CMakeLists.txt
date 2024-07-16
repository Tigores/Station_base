cmake_minimum_required(VERSION 3.10)

# set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_BUILD_TYPE "Release")

PROJECT(yolov8 VERSION 1.0.0 LANGUAGES C CXX CUDA)
message(STATUS "CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/common.cmake)
message(STATUS "TensorRT_ROOT = ${TensorRT_ROOT}")
message(STATUS "${ALL_LIBS}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 指定 CUDA 的路径
set(CUDA_INCLUDE_DIRS /usr/local/cuda-10.2/targets/aarch64-linux/include)
set(CUDA_LIB_DIRS /usr/local/cuda-10.2/targets/aarch64-linux/lib64)

# 添加包含目录和库目录
include_directories(${CUDA_INCLUDE_DIRS} ${INCLUDE_DRIS} ${OpenCV_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
link_directories(${CUDA_LIB_DIRS})

message(STATUS "INCLUDE_DRIS = ${INCLUDE_DRIS}")
message(STATUS "OpenCV_INCLUDE_DIRS = ${OpenCV_INCLUDE_DIRS}")

file(GLOB CPPS 
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cu
  ${CMAKE_CURRENT_SOURCE_DIR}/../utils/*.cu
  ${CMAKE_CURRENT_SOURCE_DIR}/../utils/*.cpp
  ${TensorRT_ROOT}/samples/common/logger.cpp 
  ${TensorRT_ROOT}/samples/common/sampleOptions.cpp
  ${TensorRT_ROOT}/samples/common/sampleUtils.cpp
  )
list(REMOVE_ITEM CPPS app_yolov8.cpp)
message(STATUS "CPPS = ${CPPS}")
list (LENGTH CPPS length)
message(STATUS "***length*** = ${length}") 

find_package(OpenCV REQUIRED)

# 添加JsonCpp库
find_package(jsoncpp REQUIRED)
include_directories(${JSONCPP_INCLUDE_DIRS})
link_directories(${JSONCPP_LIBRARY_DIRS})

# 添加cpp-httplib库
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../cpp-httplib)

add_library(${PROJECT_NAME} SHARED ${CPPS})
target_link_libraries(${PROJECT_NAME} ${ALL_LIBS} ${OpenCV_LIBRARIES} jsoncpp_lib)

set_property(TARGET ${PROJECT_NAME} PROPERTY CUDA_ARCHITECTURES 50 61 72 75)
target_compile_options(${PROJECT_NAME} PUBLIC 
   $<$<COMPILE_LANGUAGE:CUDA>:--default-stream per-thread -lineinfo; --use_fast_math --disable-warnings>)

add_executable(app_yolov8 app_yolov8.cpp)
target_link_libraries(app_yolov8 ${PROJECT_NAME} ${ALL_LIBS} ${OpenCV_LIBRARIES} jsoncpp_lib)
