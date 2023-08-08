cmake_minimum_required (VERSION 3.5)

set(PROJECT_NAME "simple-http-server")

project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -O0 -std=c++23")

set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(INC_DIR ${CMAKE_CURRENT_LIST_DIR}/api)
set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/build/third-party)

#############################################################
# tslogger library
#############################################################

set(TSLOGGER_PATH ${CMAKE_CURRENT_LIST_DIR}/third-party/tslogger)

include_directories(${TSLOGGER_PATH}/api)

add_subdirectory(${TSLOGGER_PATH})

#############################################################
# zlib library
#############################################################

set(ZLIB_PATH ${CMAKE_CURRENT_LIST_DIR}/third-party/zlib)

include_directories(${ZLIB_PATH})

add_subdirectory(${ZLIB_PATH})

#############################################################
# simple-http-server application
#############################################################

set(
	SRC_LIST
		${SRC_DIR}/main.cpp
		${SRC_DIR}/http_error.cpp
		${SRC_DIR}/tcp_server.cpp
		${SRC_DIR}/http_server.cpp
		${SRC_DIR}/utils.cpp	
		${INC_DIR}/http_error.hpp
		${INC_DIR}/tcp_connection.hpp
		${INC_DIR}/tcp_server.hpp
		${INC_DIR}/tcp_thread.hpp
		${INC_DIR}/http_server.hpp
		${INC_DIR}/utils.hpp
)

add_definitions(-DUSE_TS_LOGGER)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(
	${PROJECT_NAME}
		tslogger
		zlib
)

target_link_directories(
	${PROJECT_NAME} PRIVATE
		${LIB_DIR}/tslogger
		${LIB_DIR}/zlib
)

target_include_directories(
	${PROJECT_NAME} PRIVATE
		${INC_DIR}
)
