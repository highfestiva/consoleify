# Created by the infamous Jonas B.
C_COMPILER = gcc
CPP_COMPILER = g++

CFLAGS = -O3 -std=c++11 -fPIC -D_POSIX_PTHREAD_SEMANTICS -I../../pd/Lepra/Include -I../../pd/ThirdParty/openal-soft-1.10.622/OpenAL32/Include -I../../pd/ThirdParty/openal-soft-1.10.622/include -DPOSIX -D_XOPEN_SOURCE=600 -D_CONSOLE -DPNG_NO_ASSEMBLER_CODE -DdSingle -DdTLS_ENABLED=1 -DHAVE_CONFIG_H=1 -DLEPRA_WITHOUT_FMOD
CPPFLAGS = $(CFLAGS)  -Wall -Wno-unknown-pragmas
LIBS = -lLepra -lThirdParty -lOpenAL -lspotify -lpthread -ldl -lrt -L../../pd/ThirdParty -L../../pd/ThirdParty/openal-soft-1.10.622 -L../../pd/Lepra -L.

SRCS=	\
audio.cpp	\
openal-audio.cpp	\
playapp.cpp

OBJS=	\
audio.o	\
openal-audio.o	\
playapp.o

all:	Consoleify $(OBJS)
clean:
	@rm -f Consoleify $(OBJS)
Consoleify:	$(OBJS)
	$(CPP_COMPILER) -o $@ $(OBJS) $(LIBS)

.SUFFIXES: .o .cpp
depend:
	makedepend -- $(CPPFLAGS) -- $(SRCS)
.c.o:
	$(C_COMPILER) $(CFLAGS) -o $@ -c $<
.cpp.o:
	$(CPP_COMPILER) $(CPPFLAGS) -o $@ -c $<


