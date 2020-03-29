#https://blog.csdn.net/bigdog_1027/article/details/79113342
cmake_minimum_required(VERSION 3.1)

cmake_policy(VERSION 3.1)

#C++11
set(CMAKE_CXX_STANDARD 11)

project(X11Cap VERSION 0.3.11)

INCLUDE_DIRECTORIES(include)

set(SRC  
    ${PROJECT_SOURCE_DIR}/src/X11Input.cpp
)
# 库
#link_directories(${PROJECT_SOURCE_DIR})
add_library(xlibCap SHARED ${SRC})
target_link_libraries(xlibCap 
	X11 Xfixes Xext Xi Xinerama
	stdc++ pthread)

#可执行文件
#link_directories(${PROJECT_SOURCE_DIR})
add_executable(X11Cap ${PROJECT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(X11Cap xlibCap stdc++)