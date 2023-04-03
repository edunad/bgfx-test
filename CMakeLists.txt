﻿cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

include(FetchContent)
project ("bgfx_test" VERSION "0.0.1")

# Fix cmake paths
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
list(PREPEND CONAN_GENERATORS_FOLDER ${CMAKE_BINARY_DIR})
# ---

# repos
set(OUTPUT_BIN "bin" CACHE STRING "the target directory of where the output of the build will be stored for use by artifacts")

# flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSPDLOG_FMT_EXTERNAL -DNOMINMAX")
set(CMAKE_CXX_STANDARD 23)
# set(EXECUTABLE_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/${OUTPUT_BIN}/${CMAKE_BUILD_TYPE})

# libraries 3th party
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_INSTALL OFF CACHE INTERNAL "")

set(BGFX_BUILD_TOOLS ON CACHE INTERNAL "")
set(BGFX_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(BGFX_CUSTOM_TARGETS OFF CACHE INTERNAL "")

if(NOT WIN32)
	set(EXTRA_LIBS pthread)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
else()
	set(EXTRA_LIBS opengl32)

	# Ignore warnings about missing pdb
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /ignore:4099")
	set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4099")
endif()

set(CMAKE_SKIP_RPATH TRUE)
set(CMAKE_SKIP_BUILD_RPATH TRUE)

FetchContent_GetProperties(bgfx)
if(NOT bgfx_POPULATED)
	FetchContent_Declare(bgfx
		GIT_REPOSITORY "https://github.com/bkaradzic/bgfx.cmake.git"
		GIT_TAG "v1.118.8417-420"
		PATCH_COMMAND git reset --hard && git apply --ignore-space-change --ignore-whitespace --allow-empty "${CMAKE_CURRENT_SOURCE_DIR}/patches/bgfx-cmake.patch"
		GIT_PROGRESS TRUE
	)

	FetchContent_MakeAvailable(bgfx)
endif()

include("cmake/utils.cmake")

find_package(fmt REQUIRED)
find_package(glfw3 REQUIRED)
find_package(magic_enum REQUIRED)
find_package(glm REQUIRED)


add_shaders_directory("content/shaders" SHADERS_TARGET_NAME)
message("SHADERS: ${SHADERS_TARGET_NAME}")

file(GLOB_RECURSE source_files
	"code/src/*.cpp"
	"code/src/*.c"
	"code/include/*.hpp"
	"code/include/*.h"
)

list(REMOVE_ITEM source_files "code/src/main.cpp")

set(output_target bgfx_test)
add_executable(${output_target} ${source_files} "ias.rc")
if(NOT WIN32)
	set_target_properties(${output_target} PROPERTIES LINK_FLAGS -Wl,-rpath,'\$ORIGIN')
else()
	add_compile_options("/EHsc")
endif()
target_compile_definitions(${output_target} PUBLIC
	SOL_ALL_SAFETIES_ON=1
	_CRT_SECURE_NO_WARNINGS
	GLM_FORCE_LEFT_HANDED
	NOMINMAX
)
target_compile_features(${output_target} PUBLIC cxx_std_23)
target_link_libraries(${output_target} PUBLIC "${SHADERS_TARGET_NAME}" PRIVATE
	fmt::fmt
	glfw
	bgfx
	bx
	glm::glm
	magic_enum::magic_enum
	${EXTRA_LIBS}
)

if (WIN32)
		add_custom_command(
			TARGET ${output_target}
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/${output_target}.exe ${CMAKE_HOME_DIRECTORY}/${OUTPUT_BIN}/${CMAKE_BUILD_TYPE}/${output_target}.exe)
else()
        add_custom_command(
			TARGET ${output_target}
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/${output_target} ${CMAKE_HOME_DIRECTORY}/${OUTPUT_BIN}/${CMAKE_BUILD_TYPE}/${output_target})
endif()