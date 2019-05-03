all: 
	g++ -o bouncer.out ffmpegCommunicator.cpp -w `pkg-config --cflags --libs libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale` 
movie:
	../ffmpeg/ffmpeg -r 30 -f image2 -s 1920x1080 -i frame%d.ppm test.mp4
