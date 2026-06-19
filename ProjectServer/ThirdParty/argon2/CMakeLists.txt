cmake_minimum_required(VERSION 3.15)
project(argon2)

# Znajdź wszystkie pliki .c rekursywnie w src/
file(GLOB_RECURSE ARGON2_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Znajdź wszystkie pliki .h rekursywnie w src/ (dla private headers)
file(GLOB_RECURSE ARGON2_PRIVATE_HEADERS 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# Debug: wypisz wszystkie znalezione pliki
message(STATUS "Found Argon2 source files:")
foreach(source ${ARGON2_SOURCES})
    message(STATUS "  ${source}")
endforeach()

message(STATUS "Found Argon2 private headers:")
foreach(header ${ARGON2_PRIVATE_HEADERS})
    message(STATUS "  ${header}")
endforeach()

# Stwórz bibliotekę z wszystkimi znalezionymi plikami
add_library(argon2 STATIC ${ARGON2_SOURCES})

target_include_directories(argon2 
    PUBLIC include 
    PRIVATE src
    PRIVATE src/blake2  # Dodaj subfolder blake2 do include path
)

# Platform fixes
if(WIN32 AND MSVC)
    target_compile_definitions(argon2 PRIVATE
        _CRT_SECURE_NO_WARNINGS
        ARGON2_NO_THREADS=1
        inline=__inline
        __attribute__=
        visibility=
    )
    
    # Zastosuj flagi do wszystkich plików .c
    set_source_files_properties(${ARGON2_SOURCES} PROPERTIES COMPILE_FLAGS "/TP")
endif()

find_package(Threads REQUIRED)
target_link_libraries(argon2 Threads::Threads)