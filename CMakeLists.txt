# CMakeLists.txt — mirrors the Makefile (which is the primary build): make, make test
cmake_minimum_required(VERSION 3.20)
project(tutor CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raylib REQUIRED)
find_package(CURL REQUIRED)

file(GLOB PLACE_SRC markup/place/*.cpp)        # the placement module (pure C++, no raylib)
set(RM_SRC rm/raymanim.cpp)
set(MK_SRC ${PLACE_SRC} markup/placement.cpp markup/expr.cpp markup/script.cpp markup/engine.cpp
           markup/layout.cpp markup/render.cpp markup/tts.cpp markup/tex.cpp)

add_executable(tutor main.cpp ${RM_SRC} ${MK_SRC})
target_link_libraries(tutor raylib CURL::libcurl)
if(APPLE)
  target_link_libraries(tutor "-framework CoreVideo" "-framework IOKit" "-framework Cocoa" "-framework GLUT" "-framework OpenGL")
endif()

add_executable(manim_demo examples/manim_demo.cpp ${RM_SRC})
target_link_libraries(manim_demo raylib)

# unit tests for the placement module: built without raylib, so it can't depend on the engine
add_executable(test_place tests/test_place.cpp ${PLACE_SRC})
target_include_directories(test_place PRIVATE third_party)
enable_testing()
add_test(NAME place COMMAND test_place)
