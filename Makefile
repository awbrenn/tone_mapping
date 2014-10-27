#Makefile for lab 05
#author: Austin Brennan (awbrenn@clemson.edu)
#with help from Joshua A. Levine (levinej@clemson.edu)

#specify your compiler
CC	= g++

#first set up the platform dependent variables
#you could also add another ifeq entry for your own platform
ifeq ("$(shell uname)", "Darwin")
  #Note that these paths are based on installing OpenImageIO with
  #homebrew, hence there are no needs for the include paths
  OIIO_INC = 
  OIIO_LIB = -lOpenImageIO
  OPENGL_LIB = -framework Foundation -framework GLUT -framework OpenGL
else 
  ifeq ("$(shell uname)", "Linux")
    #On SoC machines, we need to specifically include the OIIO include
    #path in addition to the lib paths
    OIIO_INC = -I/group/dpa/include
    OIIO_LIB = -L/group/dpa/lib -lOpenImageIO
    OPENGL_LIB = -lglut -lGL -lGLU
  endif
endif

#build the LDFLAG snd CFLAGS based on variables above
LDFLAGS = $(OPENGL_LIB) $(OIIO_LIB)
CFLAGS = -g -fpermissive $(OIIO_INC)

#this will be the name of your executable
PROJECT = tonemap

#list a .o file for each .cpp file that you will compile
#this makefile will compile each cpp separately before linking
OBJECTS = tone_mapping.o

#this target does the linking step  
all: ${PROJECT}

${PROJECT}: ${OBJECTS}
	${CC} -o ${PROJECT} ${OBJECTS} ${LDFLAGS}

#this target generically compiles each .cpp to a .o file
#it does not check for .h files dependencies, but you could add that
%.o: %.cpp
	${CC} -c ${CFLAGS} *.cpp

#this will clean up all temporary files created by make all
clean:
	rm -f core.* *.o *~ ${PROJECT}
