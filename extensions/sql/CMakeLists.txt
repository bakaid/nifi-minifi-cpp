#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(../../thirdparty/rapidjson-1.1.0/include/ ../../thirdparty/rapidjson-1.1.0/include/rapidjson)
include_directories(".")

file(GLOB SOURCES  "*.cpp" "services/*.cpp" "processors/*.cpp"  "data/*.cpp")

add_library(minifi-sql STATIC ${SOURCES})
set_property(TARGET minifi-sql PROPERTY POSITION_INDEPENDENT_CODE ON)

# Get whether we should use lib64/ library paths
get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)

if(WIN32)
	find_package(ODBC REQUIRED)
else()
	# Build iODBC

	# Define byproducts
	if(LIB64 AND NOT APPLE)
		set(IODBC_BYPRODUCT "lib64/libiodbc.a")
	else()
		set(IODBC_BYPRODUCT "lib/libiodbc.a")
	endif()

	set(IODBC_BYPRODUCT_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/iodbc-install/")

	# Build project
	ExternalProject_Add(
			iodbc-external
			URL "https://github.com/openlink/iODBC/archive/v3.52.13.tar.gz"
			URL_HASH "SHA256=4bf67fc6d4d237a4db19b292b5dd255ee09a0b2daa4e4058cf3a918bc5102135"
			BUILD_IN_SOURCE true
			SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/iodbc-src"
			BUILD_COMMAND make
			CMAKE_COMMAND ""
			UPDATE_COMMAND ""
			INSTALL_COMMAND make install
			CONFIGURE_COMMAND ""
			PATCH_COMMAND ./autogen.sh && ./configure --prefix=${IODBC_BYPRODUCT_DIR}
			STEP_TARGETS build
			BUILD_BYPRODUCTS "${IODBC_BYPRODUCT_DIR}/${IODBC_BYPRODUCT}"
			EXCLUDE_FROM_ALL TRUE
	)

	# Set variables
	set(IODBC_FOUND "YES" CACHE STRING "" FORCE)
	set(IODBC_INCLUDE_DIRS "${IODBC_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
	set(IODBC_LIBRARIES "${IODBC_BYPRODUCT_DIR}/${IODBC_BYPRODUCT}" CACHE STRING "" FORCE)

	# Set exported variables for FindPackage.cmake
	set(EXPORTED_IODBC_INCLUDE_DIRS "${IODBC_INCLUDE_DIRS}" CACHE STRING "" FORCE)
	set(EXPORTED_IODBC_LIBRARIES "${IODBC_LIBRARIES}" CACHE STRING "" FORCE)

	# Create imported targets
	add_library(ODBC::ODBC STATIC IMPORTED)
	set_target_properties(ODBC::ODBC PROPERTIES IMPORTED_LOCATION "${IODBC_LIBRARIES}")
	add_dependencies(ODBC::ODBC iodbc-external)
	file(MAKE_DIRECTORY ${IODBC_INCLUDE_DIRS})
	set_property(TARGET ODBC::ODBC APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${IODBC_INCLUDE_DIRS})
endif()

# Build SOCI

# Find patch executable
find_package(Patch)

# Define patch step
set(PC "${Patch_EXECUTABLE}" -p1 -i "${CMAKE_CURRENT_SOURCE_DIR}/patch/soci.patch")

# Define byproducts
if(NOT APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(LIBSUFFIX 64)
endif()

if (WIN32)
	set(BYPRODUCT_SUFFIX "_4_0.lib")
else()
	set(BYPRODUCT_SUFFIX ".a")
endif()

set(SOCI_BYPRODUCTS
		"lib${LIBSUFFIX}/libsoci_core${BYPRODUCT_SUFFIX}"
		"lib${LIBSUFFIX}/libsoci_odbc${BYPRODUCT_SUFFIX}"
		)

set(SOCI_BYPRODUCT_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/soci-install")

foreach(SOCI_BYPRODUCT ${SOCI_BYPRODUCTS})
	list(APPEND SOCI_LIBRARIES_LIST "${SOCI_BYPRODUCT_DIR}/${SOCI_BYPRODUCT}")
endforeach(SOCI_BYPRODUCT)

# Set build options
set(SOCI_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
		"-DCMAKE_INSTALL_PREFIX=${SOCI_BYPRODUCT_DIR}"
		"-DSOCI_TESTS=OFF"
		"-DSOCI_SHARED=OFF"
		"-DSOCI_CXX_C11=ON"
		"-DWITH_ODBC=ON"
		"-DSOCI_ODBC=ON"
		"-DWITH_BOOST=OFF")

if(NOT WIN32)
	list(APPEND SOCI_CMAKE_ARGS "-DCMAKE_MODULE_PATH=${CMAKE_CURRENT_SOURCE_DIR}/cmake/"
			"-DEXPORTED_IODBC_INCLUDE_DIRS=${EXPORTED_IODBC_INCLUDE_DIRS}"
			"-DEXPORTED_IODBC_LIBRARIES=${EXPORTED_IODBC_LIBRARIES}")
endif()

# Build project
ExternalProject_Add(
		soci-external
		URL "https://github.com/SOCI/soci/archive/4.0.0.tar.gz"
		URL_HASH "SHA256=359b988d8cbe81357835317821919f7e270c0705e41951a92ac1627cb9fe8faf"
		PATCH_COMMAND ${PC}
		SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/soci-src"
		CMAKE_ARGS ${SOCI_CMAKE_ARGS}
		BUILD_BYPRODUCTS ${SOCI_LIBRARIES_LIST}
		EXCLUDE_FROM_ALL TRUE
)

# Set dependencies
if(NOT WIN32)
	add_dependencies(soci-external ODBC::ODBC)
endif()

# Set variables
set(SOCI_FOUND "YES" CACHE STRING "" FORCE)
set(SOCI_INCLUDE_DIR "${SOCI_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
set(SOCI_LIBRARIES "${SOCI_LIBRARIES_LIST}" CACHE STRING "" FORCE)

# Create imported targets
file(MAKE_DIRECTORY ${SOCI_INCLUDE_DIR})

add_library(SOCI::libsoci_core STATIC IMPORTED)
set_target_properties(SOCI::libsoci_core PROPERTIES IMPORTED_LOCATION "${SOCI_BYPRODUCT_DIR}/lib${LIBSUFFIX}/libsoci_core${BYPRODUCT_SUFFIX}")
set_target_properties(SOCI::libsoci_core PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SOCI_INCLUDE_DIR}")
add_dependencies(SOCI::libsoci_core soci-external)

add_library(SOCI::libsoci_odbc STATIC IMPORTED)
set_target_properties(SOCI::libsoci_odbc PROPERTIES IMPORTED_LOCATION "${SOCI_BYPRODUCT_DIR}/lib${LIBSUFFIX}/libsoci_odbc${BYPRODUCT_SUFFIX}")
set_target_properties(SOCI::libsoci_odbc PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SOCI_INCLUDE_DIR}")
add_dependencies(SOCI::libsoci_odbc soci-external)
set_property(TARGET SOCI::libsoci_odbc APPEND PROPERTY INTERFACE_LINK_LIBRARIES SOCI::libsoci_core)
set_property(TARGET SOCI::libsoci_odbc APPEND PROPERTY INTERFACE_LINK_LIBRARIES ODBC::ODBC)

target_link_libraries(minifi-sql SOCI::libsoci_odbc SOCI::libsoci_core)

target_link_libraries(minifi-sql ${LIBMINIFI} Threads::Threads)

SET (SQL-EXTENSION minifi-sql PARENT_SCOPE)
register_extension(minifi-sql)
