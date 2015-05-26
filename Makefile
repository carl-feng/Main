cflags= -I/usr/include/jsoncpp -Wno-write-strings -fpermissive
libs=-pthread -lboost_system -lboost_regex -lboost_thread -lcurl -ljsoncpp `pkg-config --libs opencv`

all: Main mask_bmp watchdog capturejpg captureService testOnDemandRTSPServer rtsp2jpg

mask_bmp: bitmap_image.hpp mask_bmp.cpp
	g++ -o mask_bmp mask_bmp.cpp

Main: rs232.o powermgt.o Util.o  main.o  server.o minIni.o  mongoose.o backup.o ComputerVisionInterface.o 
	g++ -o Main rs232.o powermgt.o Util.o main.o  server.o minIni.o  mongoose.o backup.o ComputerVisionInterface.o ${libs}

ComputerVisionInterface.o : ComputerVisionInterface.h ComputerVisionInterface.cpp
	g++ -c ComputerVisionInterface.cpp ${cflags}

rs232.o : rs232.h rs232.c Util.h
	gcc -c rs232.c

backup.o : backup.cpp
	g++ -c backup.cpp

powermgt.o : powermgt.h powermgt.cpp Util.h log.h
	g++ -c powermgt.cpp ${cflags}

Util.o : Util.h Util.cpp log.h
	g++ -c Util.cpp ${cflags}

main.o : main.h main.cpp Util.h log.h
	g++ -c main.cpp ${cflags}

server.o : server.cpp Util.o
	g++ -c server.cpp ${cflags}

minIni.o : minIni.h minIni.c minGlue.h
	g++ -c minIni.c ${cflags}

mongoose.o : mongoose.h mongoose.c
	g++ -c mongoose.c ${cflags}

watchdog : Util.o minIni.o watchdog.cpp log.h backup.o
	g++ -o watchdog Util.o backup.o minIni.o watchdog.cpp -lcurl -ljsoncpp ${cflags}

capturejpg : capturejpg.cpp
	g++ -o capturejpg capturejpg.cpp -lzmq ${cflags}

captureService : captureService.cpp
	g++ -o captureService captureService.cpp -ljpeg -lv4l2 -lzmq -pthread ${cflags}

rtsp2jpg : rtsp2jpg.cpp
	g++ -o rtsp2jpg rtsp2jpg.cpp -D__STDC_CONSTANT_MACROS -ljpeg -lavformat -lavfilter  -lavcodec -lswscale

testOnDemandRTSPServer : testOnDemandRTSPServer.cpp
	g++ -o testOnDemandRTSPServer testOnDemandRTSPServer.cpp -lx264 -lzmq -I/usr/local/include/liveMedia/ -I/usr/local/include/BasicUsageEnvironment/ -I/usr/local/include/groupsock/ -I/usr/local/include/UsageEnvironment  /usr/local/lib/libliveMedia.a /usr/local/lib/libgroupsock.a /usr/local/lib/libBasicUsageEnvironment.a /usr/local/lib/libUsageEnvironment.a

clean :
	rm *.o Main capturejpg captureService mask_bmp testOnDemandRTSPServer watchdog backup rtsp2jpg
