cmake_minimum_required(VERSION 3.14)

project(StealWordsMP)

# Must have at least C++14.
set(CMAKE_CXX_STANDARD 14)

# Make sure all binaries are placed into the same build folder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CUTE_FRAMEWORK_STATIC ON)

# This will download and build Cute Framework just once the first time you build your game.
# There are other ways to get cute, such as downloading and installing prebuilt binaries, or
# by building Cute Framework yourself from source and installing the binaries on your machine.
# Using FetchContent here is just the simplest way, but ultimately how you build is up to you.
# View the readme at Cute Framework for more info: https://github.com/RandyGaul/cute_framework
include(FetchContent)
FetchContent_Declare(
	cute
	GIT_REPOSITORY https://github.com/RandyGaul/cute_framework
)
FetchContent_MakeAvailable(cute)

add_executable(StealWordsMPserver
	src/main.cpp
)

add_executable(StealWordsMPclient
	src/main.cpp
)

target_include_directories(StealWordsMPserver PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_include_directories(StealWordsMPclient PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

target_compile_definitions(StealWordsMPserver PUBLIC SERVER)
target_compile_definitions(StealWordsMPclient PUBLIC CLIENT)

# Make the game link against Cute Framework.
target_link_libraries(StealWordsMPserver cute)
target_link_libraries(StealWordsMPclient cute)

# For convenience set MSVC debugger's working directory in the build folder.
# Also ask MSVC to make snake the startup project.
if (MSVC)
	set_property(TARGET StealWordsMPserver PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:StealWordsMPserver>)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT StealWordsMPserver)

	set_property(TARGET StealWordsMPclient PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:StealWordsMPclient>)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT StealWordsMPclient)
endif()