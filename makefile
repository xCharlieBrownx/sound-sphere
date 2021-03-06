CXX=g++
INCLUDES=

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
FLAGS=-D__UNIX_JACK__ -c -std=c++11
LIBS=-lasound -lpthread -ljack -lstdc++ -lm -lGL -lGLU -lglut
endif
ifeq ($(UNAME), Darwin)
FLAGS=-D__MACOSX_CORE__ -c
LIBS=-framework CoreAudio -framework CoreMIDI -framework CoreFoundation \
	-framework IOKit -framework Carbon  -framework OpenGL \
	-framework GLUT -framework Foundation \
	-framework AppKit -lstdc++ -lm
endif

OBJS=   RtAudio.o sound-sphere.o chuck_fft.o color.o

sound-sphere: $(OBJS)
	$(CXX) -o sound-sphere $(OBJS) $(LIBS)

sound-sphere.o: sound-sphere.cpp RtAudio.h chuck_fft.h
	$(CXX) $(FLAGS) sound-sphere.cpp

RtAudio.o: RtAudio.h RtAudio.cpp RtError.h
	$(CXX) $(FLAGS) RtAudio.cpp

chuck_fft.o: chuck_fft.c
	$(CXX) $(FLAGS) chuck_fft.c

color.o: color.h color.c
	$(CXX) $(FLAGS) color.c

clean:
	rm -f *~ *# *.o sound-sphere
