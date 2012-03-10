cmake_minimum_required(VERSION 2.8)

project("BTrees Perf test")

#Set the version number
set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

#Activate warnings
if(MSVC)
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -fopenmp -std=c++0x -pedantic -Wall -Wextra -Wno-long-long")
endif()

set(EXECUTABLE_OUTPUT_PATH bin)

include_directories(include)

file(
	GLOB_RECURSE
	btrees_source_files
	src/*.cpp
)

add_executable(btrees ${btrees_source_files})

target_link_libraries(btrees rt)
