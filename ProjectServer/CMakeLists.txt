# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
cmake_minimum_required (VERSION 3.1.1...4.1.1)

#==============================================================================
# Config
#==============================================================================

# Note: for android build 'main' library as it expect library libmain.so
set(PROJECT_NAME "communicatorsrv")
set(PROJECT_VERSION 0.0.1)

# Define project
project("${PROJECT_NAME}" LANGUAGES C CXX VERSION ${PROJECT_VERSION})

# Set the C++ standard to 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Crow requires these defines
add_compile_definitions(CROW_ENABLE_SSL ASIO_STANDALONE)

# Define DEBUG macro for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG=1)
else()
    add_compile_definitions(DEBUG=0)
endif()

#==============================================================================
# Dependencies
#==============================================================================
find_package(OpenSSL REQUIRED)



#==============================================================================
# ThirdParty Dependencies
#==============================================================================

# Download ASIO first (Crow depends on it)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/asio/asio/include")
    message(STATUS "Downloading ASIO to ThirdParty/asio...")
    execute_process(
        COMMAND git clone --depth 1 --branch asio-1-30-2
            https://github.com/chriskohlhoff/asio.git ThirdParty/asio
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(GIT_RESULT EQUAL 0)
        message(STATUS "ASIO downloaded successfully")
    else()
        message(FATAL_ERROR "Failed to download ASIO")
    endif()
endif()

# Set ASIO_INCLUDE_DIR for Crow
set(ASIO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/asio/asio/include" CACHE PATH "ASIO include directory")

# Download nlohmann/json
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/json/include")
    message(STATUS "Downloading nlohmann/json to ThirdParty/json...")
    execute_process(
        COMMAND git clone --depth 1 --branch v3.11.3
            https://github.com/nlohmann/json.git ThirdParty/json
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(GIT_RESULT EQUAL 0)
        message(STATUS "nlohmann/json downloaded successfully")
    else()
        message(FATAL_ERROR "Failed to download nlohmann/json")
    endif()
endif()

# Download Crow (after ASIO)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Crow/include")
    message(STATUS "Downloading Crow to ThirdParty/Crow...")
    execute_process(
        COMMAND git clone --depth 1 --branch v1.2.0
            https://github.com/CrowCpp/Crow.git ThirdParty/Crow
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(GIT_RESULT EQUAL 0)
        message(STATUS "Crow downloaded successfully")
    else()
        message(FATAL_ERROR "Failed to download Crow")
    endif()
endif()

# Configure SOCI options before fetch
set(SOCI_TESTS OFF CACHE BOOL "" FORCE)
set(SOCI_CXX11 ON CACHE BOOL "" FORCE)
set(WITH_MYSQL ON CACHE BOOL "" FORCE)
set(WITH_SQLITE3 OFF CACHE BOOL "" FORCE)
set(WITH_POSTGRESQL OFF CACHE BOOL "" FORCE)
set(WITH_ORACLE OFF CACHE BOOL "" FORCE)
set(WITH_FIREBIRD OFF CACHE BOOL "" FORCE)
set(WITH_DB2 OFF CACHE BOOL "" FORCE)
set(WITH_ODBC OFF CACHE BOOL "" FORCE)
set(WITH_EMPTY OFF CACHE BOOL "" FORCE)

# Fetch SOCI - manual git clone approach
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/soci/CMakeLists.txt")
    message(STATUS "Downloading SOCI to ThirdParty/soci...")

    # Clone SOCI directly to ThirdParty
    execute_process(
        COMMAND git clone --depth 1 --branch v4.0.3 https://github.com/SOCI/soci.git ThirdParty/soci
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(GIT_RESULT EQUAL 0)
        message(STATUS "SOCI downloaded successfully")

        # Fix cmake_minimum_required in the downloaded source
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/soci/CMakeLists.txt" SOCI_CMAKE_CONTENT)
        string(REPLACE "cmake_minimum_required(VERSION 2.8 FATAL_ERROR)"
                       "cmake_minimum_required(VERSION 3.5...4.1 FATAL_ERROR)"
                       SOCI_CMAKE_CONTENT "${SOCI_CMAKE_CONTENT}")
        file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/soci/CMakeLists.txt" "${SOCI_CMAKE_CONTENT}")
    else()
        message(FATAL_ERROR "Failed to download SOCI")
    endif()
endif()

# Add SOCI from ThirdParty
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/soci/CMakeLists.txt")
    message(STATUS "Adding SOCI from ThirdParty/soci")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/soci ${CMAKE_BINARY_DIR}/soci)
endif()

# Download uWebSockets via git clone (no FetchContent cache)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/uWebSockets/src")
    message(STATUS "Downloading uWebSockets to ThirdParty/uWebSockets...")

    execute_process(
        COMMAND git clone --depth 1 --branch v20.75.0
            --recurse-submodules --shallow-submodules
            https://github.com/uNetworking/uWebSockets.git
            ThirdParty/uWebSockets
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
    )

    if(GIT_RESULT EQUAL 0)
        message(STATUS "uWebSockets downloaded successfully")
    else()
        message(FATAL_ERROR "Failed to download uWebSockets")
    endif()
endif()

# Create convenience symlink: uwebsockets -> uWebSockets/src
# This allows #include <uwebsockets/App.h> to resolve correctly
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/uwebsockets")
    file(CREATE_LINK
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/uWebSockets/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/uwebsockets"
        SYMBOLIC
    )
endif()

# Local submodules
add_subdirectory(ThirdParty/IniReader)
add_subdirectory(ThirdParty/Encryption)

# Build argon2 from local ThirdParty source
set(ARGON2_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/argon2")
if(EXISTS "${ARGON2_SRC_DIR}")
    add_library(argon2_static STATIC
        ${ARGON2_SRC_DIR}/src/argon2.c
        ${ARGON2_SRC_DIR}/src/core.c
        ${ARGON2_SRC_DIR}/src/encoding.c
        ${ARGON2_SRC_DIR}/src/ref.c
        ${ARGON2_SRC_DIR}/src/thread.c
        ${ARGON2_SRC_DIR}/src/blake2/blake2b.c
    )
    target_include_directories(argon2_static PUBLIC
        ${ARGON2_SRC_DIR}/include
    )
    target_include_directories(argon2_static PRIVATE
        ${ARGON2_SRC_DIR}/src
    )
    set_target_properties(argon2_static PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
endif()

# SOCI is already added via add_subdirectory above

# Build uSockets from source (required by uWebSockets)
set(USOCKETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets")
if(EXISTS "${USOCKETS_DIR}/src")
    add_library(usockets_static STATIC
        ${USOCKETS_DIR}/src/bsd.c
        ${USOCKETS_DIR}/src/context.c
        ${USOCKETS_DIR}/src/loop.c
        ${USOCKETS_DIR}/src/socket.c
        ${USOCKETS_DIR}/src/udp.c
        # SSL eventing
        ${USOCKETS_DIR}/src/eventing/epoll_kqueue.c
        ${USOCKETS_DIR}/src/crypto/openssl.c
        ${USOCKETS_DIR}/src/crypto/sni_tree.cpp
    )
    target_include_directories(usockets_static PUBLIC
        ${USOCKETS_DIR}/src
    )
    target_compile_definitions(usockets_static PRIVATE
        LIBUS_USE_OPENSSL
        LIBUS_USE_EPOLL
    )
    target_link_libraries(usockets_static PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    set_target_properties(usockets_static PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
endif()

#==============================================================================
# Source directories
#==============================================================================

set(PROJECT_SOURCE_DIR          "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(PROJECT_SOURCE_PRIVATE_DIR  "${PROJECT_SOURCE_DIR}/Private")
set(PROJECT_SOURCE_PUBLIC_DIR   "${PROJECT_SOURCE_DIR}/Public")

# Set directories
set(PROJECT_BIN_DIRECTORY_PATH "${CMAKE_BINARY_DIR}/bin")
set(PROJECT_LIB_DIRECTORY_PATH "${CMAKE_BINARY_DIR}/lib")

# For all targets:
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG    ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE  ${CMAKE_BINARY_DIR}/bin/Release)

#==============================================================================
# Executable
#==============================================================================

# Collect source files
file(GLOB_RECURSE PROJECT_SOURCE_FILES_PUBLIC CONFIGURE_DEPENDS "${PROJECT_SOURCE_PUBLIC_DIR}/*.h")
file(GLOB_RECURSE PROJECT_SOURCE_FILES_PRIVATE CONFIGURE_DEPENDS "${PROJECT_SOURCE_PRIVATE_DIR}/*.cpp")

# Collect Utils sources (WebUtils .cpp files)
file(GLOB_RECURSE UTILS_SOURCES CONFIGURE_DEPENDS "Source/Utils/WebUtils/*.cpp")

message(STATUS "Adding project executable")

# Add executable
add_executable("${PROJECT_NAME}" ${PROJECT_SOURCE_FILES_PRIVATE} ${PROJECT_SOURCE_FILES_PUBLIC})

# Then set the startup project at the end
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}")

message(STATUS "Added project executable")

set_target_properties("${PROJECT_NAME}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY            "${CMAKE_BINARY_DIR}/bin/Debug/Project"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG      "${CMAKE_BINARY_DIR}/bin/Debug/Project"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE    "${CMAKE_BINARY_DIR}/bin/Release/Project"
)

#==============================================================================
# Include Directories
#==============================================================================

# Project source includes
target_include_directories("${PROJECT_NAME}" PUBLIC
    ${PROJECT_SOURCE_PUBLIC_DIR}
    ${PROJECT_SOURCE_PRIVATE_DIR}
    Source/Utils
    Source/Utils/Logger
    Source/Utils/WebUtils
    Source/Utils/Encryption
    Source/Utils/WebSocket
)

# ThirdParty includes (all local)
target_include_directories("${PROJECT_NAME}" SYSTEM PRIVATE
    # Crow (REST framework)
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Crow/include
    # asio (async I/O, needed by Crow)
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/asio/asio/include
    # nlohmann/json
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/json/include
    # SOCI (database)
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/soci/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/soci/include/soci
    ${CMAKE_BINARY_DIR}/soci/include
    ${CMAKE_BINARY_DIR}/include
    # argon2 (password hashing)
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/argon2/include
    # uWebSockets
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/uWebSockets/src
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src
)

#==============================================================================
# Sources
#==============================================================================

# Add the files to your target
target_sources("${PROJECT_NAME}"
    PUBLIC  ${PROJECT_SOURCE_FILES_PUBLIC}
    PRIVATE ${PROJECT_SOURCE_FILES_PRIVATE}
    PRIVATE ${UTILS_SOURCES}
)

# Source dirs
source_group(TREE "${PROJECT_SOURCE_DIR}"
    PREFIX "Sources"  
    FILES ${PROJECT_SOURCE_FILES_PUBLIC} ${PROJECT_SOURCE_FILES_PRIVATE}
)

#==============================================================================
# Linking
#==============================================================================

# Link libraries
target_link_libraries("${PROJECT_NAME}" PRIVATE
    IniReader
    SQRLL_Encryption
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    z
)

# Link argon2
if(TARGET argon2_static)
    target_link_libraries("${PROJECT_NAME}" PRIVATE argon2_static)
else()
    target_link_libraries("${PROJECT_NAME}" PRIVATE argon2)
endif()

# Link SOCI
if(TARGET soci_core)
    target_link_libraries("${PROJECT_NAME}" PRIVATE soci_core soci_mysql)
else()
    # Try system-installed SOCI
    find_library(SOCI_CORE_LIB soci_core)
    find_library(SOCI_MYSQL_LIB soci_mysql)
    if(SOCI_CORE_LIB AND SOCI_MYSQL_LIB)
        target_link_libraries("${PROJECT_NAME}" PRIVATE ${SOCI_CORE_LIB} ${SOCI_MYSQL_LIB})
    else()
        message(WARNING "SOCI not found - database features will not link properly")
    endif()
endif()

# Link uSockets
if(TARGET usockets_static)
    target_link_libraries("${PROJECT_NAME}" PRIVATE usockets_static)
endif()

# Link cpr (C++ HTTP requests library)
find_library(CPR_LIB cpr)
if(CPR_LIB)
    target_link_libraries("${PROJECT_NAME}" PRIVATE ${CPR_LIB})
else()
    target_link_libraries("${PROJECT_NAME}" PRIVATE cpr)
endif()

#==============================================================================
# Assets
#==============================================================================

set(ASSETS_DIR_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
if(EXISTS "${ASSETS_DIR_SOURCE_PATH}")
    add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_DIR_SOURCE_PATH}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets"
        COMMENT "Copying Assets to output directory"
    )
endif()

#==============================================================================
# IDE Support
#==============================================================================

if(WIN32)
    add_custom_target(open_vs
        COMMAND start "" "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.sln"
        COMMENT "Opening Visual Studio solution"
        USES_TERMINAL
    )
endif()
