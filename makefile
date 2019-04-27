make: 
	g++ ffmpegCommunicator.cpp -w `pkg-config --cflags --libs libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale`