cmake_minimum_required(VERSION 2.8.4)
project(lab04)

set(SOURCE_FILES tone_mapping.cpp)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    #TODO redo this with proper cmake conventions
    set(OIIO_INC "")
    set(OIIO_LIB "-lOpenImageIO")
    set(OPENGL_LIB "-framework Foundation -framework GLUT -framework OpenGL")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    include_directories("/group/dpa/include")
    find_library(OIIO "OpenImageIO" "/group/dpa/lib")
    find_library(GLUT "glut")
    find_library(GL "GL")
    find_library(GLU "GLU")
endif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

add_executable(filt ${SOURCE_FILES})
target_link_libraries(filt ${OIIO} ${GLUT} ${GL} ${GLU})