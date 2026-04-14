cmake_minimum_required(VERSION 3.15)
project(pub_manager_prototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pub_manager
    src/main.cpp
    src/game.cpp
)

target_include_directories(pub_manager PRIVATE include)

find_package(raylib 5 QUIET)
if(raylib_FOUND)
    target_link_libraries(pub_manager PRIVATE raylib)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RAYLIB REQUIRED raylib)
    target_include_directories(pub_manager PRIVATE ${RAYLIB_INCLUDE_DIRS})
    target_link_directories(pub_manager PRIVATE ${RAYLIB_LIBRARY_DIRS})
    target_link_libraries(pub_manager PRIVATE ${RAYLIB_LIBRARIES})
    target_compile_options(pub_manager PRIVATE ${RAYLIB_CFLAGS_OTHER})
endif()
