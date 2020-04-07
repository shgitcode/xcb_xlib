#https://blog.csdn.net/bigdog_1027/article/details/79113342
cmake_minimum_required(VERSION 3.1)

cmake_policy(VERSION 3.1)

#C++11
#set(CMAKE_CXX_STANDARD 11)
add_compile_options(-std=c++11)

project(X11Cap VERSION 0.3.11)

#头文件
set(INCLUDE
	include
	${PROJECT_SOURCE_DIR}/../common/include/
	${PROJECT_SOURCE_DIR}/../3rdparty/include/libyuv/
	${PROJECT_SOURCE_DIR}/../utils/include/
    ${PROJECT_SOURCE_DIR}/include/
)
INCLUDE_DIRECTORIES(${INCLUDE})

# utils library
set(YUVCONVERT_LIBRARYDIR "${PROJECT_SOURCE_DIR}/../build/utils/")

#源文件
set(SRC  
    ${PROJECT_SOURCE_DIR}/src/X11Input.cpp
)
# 库
#link_directories(${PROJECT_SOURCE_DIR})
add_library(xlibCap SHARED ${SRC})
target_link_libraries(xlibCap
	${YUVCONVERT_LIBRARYDIR}/libconvertyuv.so
	X11 Xfixes Xext Xi Xinerama
	stdc++ pthread)

#可执行文件
#link_directories(${PROJECT_SOURCE_DIR})
add_executable(X11Cap ${PROJECT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(X11Cap xlibCap stdc++)