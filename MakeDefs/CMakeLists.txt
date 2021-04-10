cmake_minimum_required(VERSION 3.0)
project(MakeDefs)

set(CMAKE_CXX_FLAGS "-std=c++14 -D_CONSOLE -DNO_RESOURCE_MANAGER -DBOOST_ALL_NO_LIB")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

set(SOURCES
  MakeDefs.cpp
  ../boost/libs/system/src/error_code.cpp
  ../client/BaseUtil.cpp
  ../client/debug.cpp
  ../client/Exception.cpp
  ../client/File.cpp
  ../client/SimpleXML.cpp
  ../client/SimpleXMLReader.cpp
  ../client/Text.cpp
)

add_executable(MakeDefs ${SOURCES})
