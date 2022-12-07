cmake_minimum_required(VERSION 3.23)
project(yourbunnywrote)

set(CMAKE_CXX_STANDARD 20)

#find_package(GLFW REQUIRED)
#include_directories(yourbunnywrote ${GLFW_INCLUDE_DIRS})
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(GLEW REQUIRED)
include_directories(yourbunnywrote
        ${SDL2_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIRS}
        ${GLUT_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIRS})

add_subdirectory(external)

add_executable(yourbunnywrote main.cpp imgui_draw.cpp imgui.cpp imgui.h imgui_internal.h imconfig.h imgui_demo.cpp imgui_tables.cpp
        imgui_widgets.cpp imstb_rectpack.h imstb_textedit.h imstb_truetype.h imgui_impl_opengl3.h imgui_impl_sdl.h
        imgui_impl_sdlrenderer.h imgui_impl_sdlrenderer.cpp imgui_impl_opengl3_loader.h imgui_impl_opengl3.cpp imgui_impl_sdl.cpp
        stb_image.h)
target_link_libraries(yourbunnywrote ${SDL2_LIBRARIES} ${OpenGL_LIBRARIES} ${GLUT_LIBRARY} ${GLEW_LIBRARY} OpenGL::GL)