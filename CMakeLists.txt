cmake_minimum_required(VERSION 3.18.4)
project (landuseoverlap)

INCLUDE(CheckIncludeFiles)
INCLUDE(CheckLibraryExists)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckCXXCompilerFlag)
	CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
if(COMPILER_SUPPORTS_CXX17)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
else()
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++17 support. Please use a different C++ compiler.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Osmium REQUIRED COMPONENTS < pbf io gdal >)

if(NOT OSMIUM_FOUND)
	message(WARNING "Libosmium not found!\n")
endif()

find_package(Boost CONFIG 1.70.0 REQUIRED COMPONENTS program_options )
include_directories(${Boost_INCLUDE_DIRS})

find_library(SPATIAL_LIBRARY spatialindex)
find_path(SPATIAL_INCLUDE_DIR spatialindex/SpatialIndex.h)
if(SPATIAL_LIBRARY AND SPATIAL_INCLUDE_DIR)
	message(STATUS "SpatialIndex AreaStore backend enabled.")
endif(SPATIAL_LIBRARY AND SPATIAL_INCLUDE_DIR)

include_directories(${SPATIAL_INCLUDE_DIR})

add_executable(landuseoverlap landuseoverlap.cpp SpatiaLiteWriter.cpp Area.cpp AreaIndex.cpp)
target_link_libraries(landuseoverlap ${SPATIAL_LIBRARY})

include_directories("${OSMIUM_INCLUDE_DIRS}")
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(landuseoverlap ${OSMIUM_LIBRARIES} ${Boost_LIBRARIES})

