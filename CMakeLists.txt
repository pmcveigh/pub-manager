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

find_package(SDL2 QUIET)
if(SDL2_FOUND)
    target_include_directories(pub_manager PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(pub_manager PRIVATE ${SDL2_LIBRARIES})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    target_include_directories(pub_manager PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_directories(pub_manager PRIVATE ${SDL2_LIBRARY_DIRS})
    target_link_libraries(pub_manager PRIVATE ${SDL2_LIBRARIES})
    target_compile_options(pub_manager PRIVATE ${SDL2_CFLAGS_OTHER})
endif()
