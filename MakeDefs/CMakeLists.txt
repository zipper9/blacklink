cmake_minimum_required(VERSION 3.0)
project(MakeDefs)

set(CMAKE_CXX_FLAGS "-std=c++14 -D_CONSOLE -DNO_RESOURCE_MANAGER -DBOOST_ALL_NO_LIB")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

if(APPLE)
  add_compile_definitions(_DARWIN_C_SOURCE)
endif()

set(SOURCES
  MakeDefs.cpp
  ../client/BaseUtil.cpp
  ../client/debug.cpp
  ../client/Exception.cpp
  ../client/File.cpp
  ../client/SimpleXML.cpp
  ../client/SimpleXMLReader.cpp
  ../client/Text.cpp
)

include_directories(../boost)
add_executable(MakeDefs ${SOURCES})

find_library(ICONV_LIB iconv)
if(ICONV_LIB)
  target_link_libraries(MakeDefs ${ICONV_LIB})
endif()
