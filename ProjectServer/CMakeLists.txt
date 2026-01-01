# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
cmake_minimum_required (VERSION 3.1.1...4.1.1)

#==============================================================================
# Config
#==============================================================================

# Options setup
option(USE_CUSTOM_ENGINE "Should we use custom engine?" OFF) # OFF for CI

set(ENGINE_USING_VIDEO OFF CACHE BOOL "Disable video" FORCE)
set(ENGINE_USING_AUDIO OFF CACHE BOOL "Disable audio" FORCE)

# Note: for android build 'main' library as it expect library libmain.so
set(PROJECT_NAME "communicatorsrv")
set(PROJECT_VERSION 0.0.1)

set(ENGINE_PATH_DEFAULT "../../Engine/Engine")

if(WIN32)
    set(ENGINE_PATH_CUSTOM "C:/Repos/Engine/Engine")
elseif(UNIX)
    set(ENGINE_PATH_CUSTOM "/var/repos/Engine/Engine")
endif()

# Define project
project("${PROJECT_NAME}" LANGUAGES CXX VERSION ${PROJECT_VERSION})

# Set the C++ standard to 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENGINE_USING_VIDEO "Is this app using any video features? (liek SDL Video)" OFF)

# Generated config
set(ENGINE_BUILD_PATH "${CMAKE_BINARY_DIR}/Engine_build")

# @TODO: Force diasbled lib gme due to android issues but it should be fixed
set(SDLMIXER_GME OFF CACHE BOOL "Disable support for GME music" FORCE)
set(SDLMIXER_GME_SHARED OFF CACHE BOOL "Disable dynamic loading of libgme" FORCE)

# Make sure engine compiles as library
set(ENGINE_BUILD_AS_LIBRARY ON CACHE BOOL "ON - build as library, OFF - build as executable" FORCE)

# Check which engine to use
if(USE_CUSTOM_ENGINE)
	set(ENGINE_PATH "${ENGINE_PATH_CUSTOM}")
else()
	set(ENGINE_PATH "${ENGINE_PATH_DEFAULT}")
endif()

#==============================================================================
# Executable (Except android which is linked into sample SDL project)
#==============================================================================

# Depending on platform we want to add executable or build as library
if(NOT ANDROID)
	message(STATUS "Adding project executable")

	# Add executable
	add_executable("${PROJECT_NAME}" ${PROJECT_SOURCE_FILES_PRIVATE} ${PROJECT_SOURCE_FILES_PUBLIC})

	# Then set the startup project at the end
	set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

	message(STATUS "Added project executable")
else()
	# Project library
	add_library("${PROJECT_NAME}" SHARED 
		${PROJECT_SOURCE_FILES_PRIVATE}
		${PROJECT_SOURCE_FILES_PUBLIC}
	)

	# The SDL java code is hardcoded to load libmain.so on android, so we need to change EXECUTABLE_NAME
	# Other platforms could have different lib name but found no reason so far to rename?
	set(EXECUTABLE_NAME main)

	message(STATUS "Android executable name set (main)")
endif()

#==============================================================================
# Engine 
#==============================================================================

# Engine directory
add_subdirectory(${ENGINE_PATH} ${ENGINE_BUILD_PATH})

# Engine config
set(ENGINE_COPY_ENGINE_MAPS OFF)

get_property(ENGINE_CONFIGURATION GLOBAL PROPERTY ENGINE_CONFIGURATION)
get_property(RUNTIME_LIB_EXT GLOBAL PROPERTY RUNTIME_LIB_EXT)

set(PROJECT_SOURCE_DIR          "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(PROJECT_SOURCE_PRIVATE_DIR  "${PROJECT_SOURCE_DIR}/Private")
set(PROJECT_SOURCE_PUBLIC_DIR   "${PROJECT_SOURCE_DIR}/Public")

# Set directories
set(PROJECT_BIN_DIRECTORY_PATH "${CMAKE_BINARY_DIR}/bin")
set(PROJECT_LIB_DIRECTORY_PATH "${CMAKE_BINARY_DIR}/lib")

# For all targets:
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG    ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE  ${CMAKE_BINARY_DIR}/bin/Release)

set_target_properties("${PROJECT_NAME}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY            "${CMAKE_BINARY_DIR}/bin/Debug/Project"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG      "${CMAKE_BINARY_DIR}/bin/Debug/Project"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE    "${CMAKE_BINARY_DIR}/bin/Release/Project"
)

# Assets
get_property(ASSETS_DIR_NAME GLOBAL PROPERTY ASSETS_DIR_NAME)
get_property(ASSETS_ENGINE_TARGET_PATH GLOBAL PROPERTY ASSETS_ENGINE_TARGET_PATH)
set(ASSETS_DIR_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${ASSETS_DIR_NAME})

# Get assets
file(GLOB ASSET_FILES_TO_COPY "${ASSETS_DIR_SOURCE_PATH}/*")

# Print assets dirs
message(STATUS "COPY PROJECT ASSETS: '${ASSETS_DIR_SOURCE_PATH}', DESTINATION: '${ASSETS_ENGINE_TARGET_PATH}'")

# Copy file before build
CopyAssetFiles("${ASSET_FILES_TO_COPY}" "${ASSETS_ENGINE_TARGET_PATH}")

# Copy files after build (Only then we know if it's debug/release)
CopyAssetsPostBuild("${PROJECT_NAME}" "${ASSETS_ENGINE_TARGET_PATH}")

# Enable Hot Reload for MSVC compilers if supported.
# TODO: Check if it works, should be enabled by child project
#if (POLICY CMP0141)
#  cmake_policy(SET CMP0141 NEW)
#  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
#endif()

# Add source files for project (by directory)
target_include_directories("${PROJECT_NAME}"
    PUBLIC      ${PROJECT_SOURCE_PUBLIC_DIR}
    PRIVATE     ${PROJECT_SOURCE_PRIVATE_DIR}
)

# Collect source files
file(GLOB_RECURSE PROJECT_SOURCE_FILES_PUBLIC CONFIGURE_DEPENDS "${PROJECT_SOURCE_PUBLIC_DIR}/*.h")
file(GLOB_RECURSE PROJECT_SOURCE_FILES_PRIVATE CONFIGURE_DEPENDS "${PROJECT_SOURCE_PRIVATE_DIR}/*.cpp")

# Add the files to your target
target_sources("${PROJECT_NAME}" 
    PUBLIC  ${PROJECT_SOURCE_FILES_PUBLIC}
    PRIVATE ${PROJECT_SOURCE_FILES_PRIVATE}
)

# Source dirs
source_group(TREE "${PROJECT_SOURCE_DIR}"
    PREFIX "Sources"  
    FILES ${PROJECT_SOURCE_FILES_PUBLIC} ${PROJECT_SOURCE_FILES_PRIVATE}
)

# Link engine
target_link_libraries("${PROJECT_NAME}" PRIVATE Engine)

# Copy Engine library to output directory
# Must be done after linking!
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Engine>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying Engine shared object to output directory"
)

#==============================================================================
# TESTS
#==============================================================================

set(PROJECT_TESTS_ENABLED FALSE)

# Decide if we want tests (for now only windows x64)
if(WIN32 AND NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    if(CMAKE_GENERATOR_PLATFORM STREQUAL "x64" OR (NOT DEFINED CMAKE_GENERATOR_PLATFORM AND CMAKE_SIZEOF_VOID_P EQUAL 8))
        set(PROJECT_TESTS_ENABLED TRUE)
    endif()
endif()

if(PROJECT_TESTS_ENABLED)
    message(STATUS "Adding google tests")
    
    set(TESTPROJECT_SOURCE_DIR          "${CMAKE_CURRENT_SOURCE_DIR}/SourcesTest")
    
    set(TESTPROJECT_SOURCE_PUBLIC_DIR   "${TESTPROJECT_SOURCE_DIR}/Public")
    set(TESTPROJECT_SOURCE_PRIVATE_DIR  "${TESTPROJECT_SOURCE_DIR}/Private")

    # Collect test source files
	file(GLOB ESTPROJECT_SOURCE_FILES CONFIGURE_DEPENDS "${TESTPROJECT_SOURCE_DIR}/*.cpp" "${TESTPROJECT_SOURCE_DIR}/*.h")

    file(GLOB_RECURSE TESTPROJECT_SOURCE_FILES_PUBLIC CONFIGURE_DEPENDS "${TESTPROJECT_SOURCE_PUBLIC_DIR}/*.h")
    file(GLOB_RECURSE TESTPROJECT_SOURCE_FILES_PRIVATE CONFIGURE_DEPENDS "${TESTPROJECT_SOURCE_PRIVATE_DIR}/*.cpp")

    # Create test executable
    add_executable(ProjectTests)

    set_target_properties(ProjectTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY            "${CMAKE_BINARY_DIR}/bin/Debug/Test"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG      "${CMAKE_BINARY_DIR}/bin/Debug/Test"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE    "${CMAKE_BINARY_DIR}/bin/Release/Test"
    )

	# Link test libraries
	target_link_libraries(ProjectTests
		PUBLIC Engine
		PUBLIC GTest::gtest_main
	)

    # Add the files to your target
    target_sources(ProjectTests
        PRIVATE		${PROJECT_SOURCE_FILES_PUBLIC}
        PRIVATE		${PROJECT_SOURCE_FILES_PRIVATE}
		
		# Test project
	    PRIVATE     ${ESTPROJECT_SOURCE_FILES}
        PRIVATE     ${TESTPROJECT_SOURCE_FILES_PUBLIC}
        PRIVATE     ${TESTPROJECT_SOURCE_FILES_PRIVATE}
    )

    # Include directories
    target_include_directories(ProjectTests PRIVATE
	    # Project source files (for testing)
	    ${PROJECT_SOURCE_DIR}  
	    ${PROJECT_SOURCE_PUBLIC_DIR}
	    ${PROJECT_SOURCE_PRIVATE_DIR} 
		
		# Test project
		${TESTPROJECT_SOURCE_DIR}
	    ${TESTPROJECT_SOURCE_PUBLIC_DIR}
	    ${TESTPROJECT_SOURCE_PRIVATE_DIR} 
    )
	
	source_group(TREE "${TESTPROJECT_SOURCE_DIR}" 
		PREFIX "SourceTest"
		FILES ${TESTPROJECT_SOURCE_FILES_PUBLIC} ${TESTPROJECT_SOURCE_FILES_PRIVATE} ${ESTPROJECT_SOURCE_FILES}
	)
	
	# Skip project run as we run tests instead
	# A bit hacky but simple
	target_compile_definitions(ProjectTests PRIVATE
		FLAGS_TESTING_PROJECT
	)


	CopyAssetsPostBuild(ProjectTests "${ASSETS_ENGINE_TARGET_PATH}")

    # Discover and register tests
    include(GoogleTest)

    # Copy Engine library to output directory
    # Must be done after linking!
    add_custom_command(TARGET ProjectTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Engine>
            $<TARGET_FILE_DIR:ProjectTests>
        COMMENT "Copying Engine libraries to output directory"
    )

    message(STATUS "Added google tests!")
endif()

if(PROJECT_TESTS_ENABLED)
    # Target for build all (To use from config script)
    add_custom_target(BuildAllProject ALL
        DEPENDS Engine "${PROJECT_NAME}" ProjectTests
    )
else()
    # Target for build all (To use from config script)
    add_custom_target(BuildAllProject ALL
        DEPENDS Engine "${PROJECT_NAME}"
    )
endif()

if(WIN32)
    add_custom_target(open_vs
        COMMAND start "" "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.sln"
        COMMENT "Opening Visual Studio solution"
        USES_TERMINAL
    )
endif()

# Use it to copy engine libraries
copy_runtime_engine_dlls("${PROJECT_NAME}")

group_targets("Core" "${PROJECT_NAME}")

if(PROJECT_TESTS_ENABLED)
    # Use it to copy engine libraries
    copy_runtime_engine_dlls(ProjectTests)

    # Special test dlls copy
    add_custom_command(TARGET ProjectTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:gtest> $<TARGET_FILE_DIR:ProjectTests>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:gtest_main> $<TARGET_FILE_DIR:ProjectTests>
    )

    # Discover after DLLs are copied
    add_test(NAME ProjectTests COMMAND ProjectTests)

	group_targets("Test" ProjectTests)

    message(STATUS "Discovered gtests")
endif()

# Then group everything else
group_remaining_targets("Other")
