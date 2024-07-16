cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_CXX_STANDARD 14)

# 包含头文件目录
include_directories(/home/jetson/dxh/C/Include/Common)    
# 库文件目录              
link_directories(/home/jetson/dxh/C/Bin)                                

# 查找 jsoncpp 包
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

# 设置 jsoncpp 包路径
include_directories(${JSONCPP_INCLUDE_DIRS})
link_directories(${JSONCPP_LIBRARY_DIRS})

# 添加 httplib 的头文件目录
include_directories(/home/jetson/dxh/TensorRT-Alpha-main/cpp-httplib)  

add_executable(my_program test.cpp)                                     # 添加可执行文件

# 链接库文件
target_link_libraries(my_program dhconfigsdk dhnetsdk StreamConvertor ${JSONCPP_LIBRARIES})  


