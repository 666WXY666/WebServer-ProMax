cmake_minimum_required(VERSION 3.15)
project(WebServer)

set(CMAKE_CXX_STANDARD 14)
# 指定执行程序的输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 添加寻找FindSFML.cmake文件的路径
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmakeModule/;${CMAKE_MODULE_PATH};")
# 开启调试，如不需要开启就把下面三行注释
# SET(CMAKE_BUILD_TYPE "Debug")
# SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
# SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

# 查找第三方库
find_package(Threads REQUIRED)
find_package(MySQL REQUIRED)

aux_source_directory(./codes DIR_FILE)

add_executable(WebServer ${DIR_FILE} main.cpp)

# 添加头文件所在路径，这时候cpp中就可以直接引用，而不用管路径了
target_include_directories(WebServer PUBLIC ${PROJECT_SOURCE_DIR}/headers)
# 添加pthread，mysql支持
target_link_libraries(WebServer PUBLIC Threads::Threads ${MYSQL_LIB})
include_directories(${MYSQL_INCLUDE_DIR})