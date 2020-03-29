#https://blog.csdn.net/bigdog_1027/article/details/79113342
#https://blog.csdn.net/afei__/article/details/81201039?depth_1-utm_source=
#distribute.pc_relevant.none-task&utm_source=distribute.pc_relevant.none-task

cmake_minimum_required(VERSION 3.1)

cmake_policy(VERSION 3.1)

#C++11
set(CMAKE_CXX_STANDARD 11)

project(XcbCap VERSION 0.3.11)

#ͷ�ļ�
INCLUDE_DIRECTORIES(include)

#XCb�����
set(LINK_LIB_LIST xcb)

#�Ƿ���libxcb-shm��
find_library(use-shm xcb-shm /usr/lib /usr/local/lib NO_DEFAULT_PATH)

if(NOT use-shm)
	message(warning  "libxcb-shm not found")
	option(WITH_XCB_SHM "Build with xcb_shm support." FALSE)
else()
	message("libxcb-shm has been found: ${use-shm}")
	option(WITH_XCB_SHM "Build with xcb_shm support." TRUE)
	set(LINK_LIB_LIST ${LINK_LIB_LIST} ${use-shm})
endif()

#�Ƿ���libxcb-xfixes��
find_library(use-xfixes xcb-xfixes)

if(NOT use-xfixes)
	message(warning "libxcb-xfixes not found")
	option(WITH_XCB_XFIXES "Build with xcb_xfixes support." FALSE)
else()
	message("libxcb-xfixes has been found: ${use-xfixes}")
	option(WITH_XCB_XFIXES "Build with xcb_xfixes support." TRUE)
	set(LINK_LIB_LIST ${LINK_LIB_LIST} ${use-xfixes})
endif()

#Դ�ļ�
set(SRC  
    ${PROJECT_SOURCE_DIR}/src/XcbInput.cpp
)

#���ɿ�
#link_directories(${PROJECT_SOURCE_DIR})
add_library(xcbLibCap SHARED ${SRC})

#message(${WITH_XCB_XFIXES})

message("xcb library : ${LINK_LIB_LIST}")

target_link_libraries(xcbLibCap 
	${LINK_LIB_LIST} xcb-shape
	stdc++ pthread)

#���Ӻ�
target_compile_definitions(xcbLibCap PRIVATE
	-DCONFIG_LIBXCB_SHM=$<BOOL:${WITH_XCB_SHM}>
	-DCONFIG_LIBXCB_XFIXES=$<BOOL:${WITH_XCB_XFIXES}
	-DCONFIG_LIBXCB_SHAPE=$<BOOL:${WITH_XCB_SHAPE}>
) 

#���ɿ�ִ���ļ�
#link_directories(${PROJECT_SOURCE_DIR})
add_executable(XcbCap ${PROJECT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(XcbCap xcbLibCap stdc++)