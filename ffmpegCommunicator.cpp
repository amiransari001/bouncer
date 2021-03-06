#include <string>
#include <iostream>
#include <stdio.h>

extern "C"
{
#include "../ffmpeg/libavcodec/avcodec.h"
#include "../ffmpeg/libavformat/avformat.h"
#include "../ffmpeg/libswscale/swscale.h"
#include "../ffmpeg/libavutil/imgutils.h"
}

using namespace std;

void DrawBall(AVFrame *pFrame, int width, int height, int iFrame) 
{
  int radius = 0; 

  // Makes ball size proportional to smallest dimension. 
  if (width < height)
  {
      radius = width/10;
  }
  else
  {
      radius = height/10;
  }

  int ballX = width/2;
  int ballY = -1 * cos(iFrame) * (height/8*3 - radius) + (height/4*3);

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      int offset = 3 * (x + y * width);

      int distance = sqrt(pow(ballX - x, 2) + pow(ballY - y, 2));

      if (distance <= radius)
      {
        int shadeX = ballX + (radius/2);
        int shadeY = ballY - (radius/2);

        int shadeDegree = sqrt(pow(shadeX - x, 2) + pow(shadeY - y, 2));
        shadeDegree = (radius*2) - shadeDegree;
        
        pFrame->data[0][offset + 0] = shadeDegree; // R
        pFrame->data[0][offset + 1] = shadeDegree; // G
        pFrame->data[0][offset + 2] = shadeDegree; // B
      } 
      
    }
  }
}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) 
{
  FILE *pFile;
  char szFilename[32];
  int  y;
  
  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;
  
  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  
  // Write pixel data
  for(y=0; y<height; y++)
  {
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
  }
  
  // Close file
  fclose(pFile);
}

void SaveFrameCool(AVFrame *pFrame, AVCodecContext *otherCtx, int iFrame) 
{
  //Initializing Variables
  AVCodec *codec;
  AVCodecContext *av_context = NULL;
  AVPacket av_packet;
  int *gotPkt;
  AVFrame *frame;
  FILE *pFile;
  char szFilename[32];

  //Getting the codec and context set up
  codec = avcodec_find_encoder(AV_CODEC_ID_COOL);
  av_context = avcodec_alloc_context3(codec);
  
  //Setting values for context
  av_context->width = otherCtx->width;
  av_context->height = otherCtx->height;
  av_context-> time_base= (AVRational){1,25};
  av_context->pix_fmt = AV_PIX_FMT_RGB24;

  avcodec_open2(av_context, codec, NULL);

  // Open file
  sprintf(szFilename, "frames/coolFrame%d.cool", iFrame);
  pFile = fopen(szFilename, "wb");

  //Initialize packet info
  av_init_packet(&av_packet);
  av_packet.data = NULL;
  av_packet.size == 0;

  std::cout << "Pre-fault" << std::endl;

  //Encode the data
  int out_size = avcodec_encode_video2(av_context, &av_packet, pFrame, gotPkt);

  //Write the data
  if(gotPkt)
  {
    fwrite(av_packet.data, 1, av_packet.size, pFile);
    av_free_packet(&av_packet);
  }
  
  // Close file
  fclose(pFile);
}


int main(int argc, char *argv[]) 
{
    // Initalizing these to NULL prevents segfaults!
    AVFormatContext   *pFormatCtx = NULL;
    int               i, videoStream;
    AVCodecContext    *pCodecCtxOrig = NULL;
    AVCodecContext    *pCodecCtx = NULL;
    AVCodec           *pCodec = NULL;
    AVFrame           *pFrame = NULL;
    AVFrame           *pFrameRGB = NULL;
    AVPacket          packet;
    int               frameFinished;
    int               numBytes;
    uint8_t           *buffer = NULL;
    struct SwsContext *sws_ctx = NULL;

    if(argc < 2) {
      printf("Please provide a movie file\n");
      return -1;
    }

    // Register all components of FFmpeg
    av_register_all();

    
    // Open video file
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
    {
        return -1; // Couldn't open file
    }
    
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    {
        return -1; // Couldn't find stream information
    }

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    // Find the first video stream
    videoStream = -1;
    for(int i=0; i<pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) 
        {
            videoStream = i;
            break;
        }
    }
        
    if(videoStream==-1)
        return -1; // Didn't find a video stream

    // Get a pointer to the codec context for the video stream
    pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;

    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec==NULL) {
      fprintf(stderr, "Unsupported codec!\n");
      return -1; // Codec not found
    }

    // Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
      fprintf(stderr, "Couldn't copy codec context");
      return -1; // Error copying codec context
    }

    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
      return -1; // Could not open codec
    
    // Allocate video frame
    pFrame=av_frame_alloc();

    
    // Allocate an AVFrame structure
    pFrameRGB=av_frame_alloc();
    if(pFrameRGB==NULL)
        return -1;

    // Determine required buffer size and allocate buffer
    numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
    buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));


    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
                    pCodecCtx->width, pCodecCtx->height);





    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width,
			     pCodecCtx->height,
			     pCodecCtx->pix_fmt,
			     pCodecCtx->width,
			     pCodecCtx->height,
			     AV_PIX_FMT_RGB24,
			     SWS_BILINEAR,
			     NULL,
			     NULL,
			     NULL);

    // What does this while loop do.
    while(av_read_frame(pFormatCtx, &packet)>=0) 
      {
	// Is this a packet from the video stream?
	if(packet.stream_index==videoStream) 
	  {
	    // Decode video frame
	    avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
          
	    // Did we get a video frame?
	    if(frameFinished) 
	      {
		// Convert the image from its native format to RGB
		sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
			  pFrame->linesize, 0, pCodecCtx->height,
			  pFrameRGB->data, pFrameRGB->linesize);
		for(i = 0; i<300; i++)
		{
		  AVFrame *drawPrintFrame = av_frame_alloc();
		  numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
		  buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

		  avpicture_fill((AVPicture *)drawPrintFrame, buffer, AV_PIX_FMT_RGB24,
				 pCodecCtx->width, pCodecCtx->height);
		  avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
		  sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
			    pFrame->linesize, 0, pCodecCtx->height,
			    drawPrintFrame->data, drawPrintFrame->linesize);
		  DrawBall(drawPrintFrame, pCodecCtx->width, pCodecCtx->height, i);
		  SaveFrame(drawPrintFrame, pCodecCtx->width, pCodecCtx->height, i);
		}
          
	      }
	  }
    
	// Free the packet that was allocated by av_read_frame
	av_free_packet(&packet);
      }

    // Free the RGB image
    av_free(buffer);
    av_free(pFrameRGB);

    // Free the YUV frame
    av_free(pFrame);

    // Close the codecs
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0; 
}
