cmake_minimum_required(VERSION 2.8)
include_directories(../../include)
IF(MSVC)
add_definitions( "/wd4005 /wd4996 /nologo" )
ENDIF(MSVC)

IF(CMAKE_COMPILER_IS_GNUCXX)
add_definitions("-std=c++11")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)
LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/lib)
add_executable(opengl main.cpp)
IF(MSVC)
target_link_libraries(opengl image ogui demo
  debug glew32s_d 
  debug freeglut_static_d
  optimized glew32s 
  optimized freeglut_static)
ENDIF(MSVC)
IF(CMAKE_COMPILER_IS_GNUCXX)
target_link_libraries(opengl image ogui demo glut GL GLEW)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)
