#https://blog.csdn.net/bigdog_1027/article/details/79113342
#https://blog.csdn.net/afei__/article/details/81201039?depth_1-utm_source=
#distribute.pc_relevant.none-task&utm_source=distribute.pc_relevant.none-task

cmake_minimum_required(VERSION 3.0)

cmake_policy(VERSION 3.0)

#C++11
#set(CMAKE_CXX_STANDARD 11)
add_compile_options(-std=c++11)

project(yuvtest VERSION 0.3.11)

#头文件
set(INCLUDE  
	${PROJECT_SOURCE_DIR}/../common/include/
	${PROJECT_SOURCE_DIR}/../3rdparty/include/libyuv/
    ${PROJECT_SOURCE_DIR}/include/
)
INCLUDE_DIRECTORIES(${INCLUDE})

#库变量
set(YUV_INCLUDEDIR "${PROJECT_SOURCE_DIR}/../3rdparty/include/libyuv/")
set(YUV_LIBRARYDIR "${PROJECT_SOURCE_DIR}/../3rdparty/lib")

message(yuvtest "${PROJECT_SOURCE_DIR}")
#源文件
set(SRC  
    ${PROJECT_SOURCE_DIR}/src/convertYuv.cpp
)

#生成库
add_library(convertyuv SHARED ${SRC})

# 链接指定的yuv头文件。
target_include_directories( convertyuv
	PRIVATE 
	${YUV_INCLUDEDIR})
# 链接指定的yuv库文件。
target_link_libraries( convertyuv
	PRIVATE
	${YUV_LIBRARYDIR}/libyuv.so
	stdc++)

#生成可执行文件
#link_directories(${PROJECT_SOURCE_DIR})
#add_executable(yuvtest ${PROJECT_SOURCE_DIR}/src/main.cpp)
#target_link_libraries(yuvtest convertyuv stdc++)
