CC=gcc
CXX=g++
RM= /bin/rm -vf
ARCH=UNDEFINED
PWD=pwd
CDR=$(shell pwd)

EDCFLAGS:=$(CFLAGS)
EDLDFLAGS:=$(LDFLAGS)
EDDEBUG:=$(DEBUG)

ifeq ($(ARCH),UNDEFINED)
	ARCH=$(shell uname -m)
endif

UNAME_S := $(shell uname -s)

EDCFLAGS+= -I include/ -I drivers/ -I ./ -Wall -O3 -std=gnu11 -DADIDMA_NOIRQ -D_POSIX_SOURCE -I libs/gl3w -DIMGUI_IMPL_OPENGL_LOADER_GL3W
CXXFLAGS:= -I include/ -I imgui/include/ -I ./ -Wall -O3 -fpermissive -std=gnu++11 -I libs/gl3w -DIMGUI_IMPL_OPENGL_LOADER_GL3W
# EDCFLAGS+= -DRXDEBUG -DTXDEBUG -DADIDMA_DEBUG -DIIO_DEBUG
CXXFLAGS+= -DENABLE_MODEM -DLIBIIO_FTR_FILE
EDLDFLAGS += -lpthread -lm -liio
LIBS = 

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += -lGL `pkg-config --static --libs glfw3`
	LIBEXT= so
	LINKOPTIONS:= -shared
	CXXFLAGS += `pkg-config --cflags glfw3`
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBEXT= dylib
	LINKOPTIONS:= -dynamiclib -single_module
	CXXFLAGS:= -arch $(ARCH) $(CXXFLAGS)
	LIBS += -arch $(ARCH) -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LIBS += -L/usr/local/lib -L/opt/local/lib
	#LIBS += -lglfw3
	LIBS += -lglfw

	CXXFLAGS += -I/usr/local/include -I/opt/local/include
	CFLAGS = $(CXXFLAGS)
endif

LIBS += -lpthread -lm -liio

LIBTARGET=imgui/libimgui_glfw.a

CPPOBJS=src/guimain.o

MESCLKOBJS=src/measureclk.o \
	src/libuio.o

COBJS=src/adidma.o \
	src/libiio.o \
	src/libuio.o \
	src/txmodem.o \
	src/rxmodem.o

TXOBJS=src/txtest.o
RXOBJS=src/rxtest.o

PHTX=src/txphoto.o
PHRX=src/txphoto.o

GUITARGET=phy.out
TXTARGET=tx.out
RXTARGET=rx.out

TXPHOTO=txphoto.out
RXPHOTO=rxphoto.out

all: $(LIBTARGET) $(GUITARGET) $(TXTARGET) $(RXTARGET) $(TXPHOTO) $(RXPHOTO)

modemlib: $(COBJS)
	ar -crus libmodem.a $(COBJS)

$(GUITARGET): $(LIBTARGET) $(COBJS) $(CPPOBJS)
	$(CXX) -o $@ $(COBJS) $(CPPOBJS) $(LIBTARGET) $(LIBS)

$(TXTARGET): $(COBJS) $(TXOBJS)
	$(CC) -o $@ $(COBJS) $(TXOBJS) $(EDLDFLAGS)

$(RXTARGET): $(COBJS) $(RXOBJS)
	$(CC) -o $@ $(COBJS) $(RXOBJS) $(EDLDFLAGS)

$(LIBTARGET): imgui
	cd imgui && make && cd ..

$(TXPHOTO): $(COBJS) $(PHTX)
	$(CC) -o $@ $(COBJS) $(PHTX) $(EDLDFLAGS) 

$(RXPHOTO): $(COBJS) $(PHRX)
	$(CC) -o $@ $(COBJS) $(PHRX) $(EDLDFLAGS) 

fixdt:
	$(CC) -o $@.out -O2 -I include/ src/test_fixdt.c -lm

mesclk: $(MESCLKOBJS) $(LIBTARGET)
	$(CXX) -o $@.out $(CXXFLAGS) $(MESCLKOBJS) $(LIBTARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<
%.o: %.c
	$(CC) $(EDCFLAGS) -o $@ -c $<
	
.PHONY: clean

cleanobjs:
	$(RM) $(COBJS)
	$(RM) $(RXOBJS)
	$(RM) $(TXOBJS)
	$(RM) $(CPPOBJS)
	$(RM) $(MESCLKOBJS)
	$(RM) $(PHTX)
	$(RM) $(PHRX)

clean: cleanobjs
	$(RM) *.out
	$(RM) *.a

spotless: clean
	cd imgui && make spotless && cd ..
