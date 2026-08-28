cmake_minimum_required(VERSION 3.24)

project(AbyssalArena VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(arena_core
    src/core/GameWorld.cpp
)

target_include_directories(arena_core PUBLIC src)

if(MSVC)
    target_compile_options(arena_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(arena_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

if(WIN32)
    add_executable(abyssal_arena WIN32
        src/app/AudioSystem.cpp
        src/app/WinMain.cpp
    )
    target_compile_definitions(abyssal_arena PRIVATE NOMINMAX UNICODE _UNICODE)
    target_link_libraries(abyssal_arena PRIVATE arena_core user32 gdi32 winmm)

    if(MSVC)
        target_compile_options(abyssal_arena PRIVATE /W4 /permissive-)
    else()
        target_compile_options(abyssal_arena PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(arena_core_tests tests/GameWorldTests.cpp)
    target_link_libraries(arena_core_tests PRIVATE arena_core)
    if(MSVC)
        target_compile_options(arena_core_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(arena_core_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
    add_test(NAME arena_core_tests COMMAND arena_core_tests)
endif()
