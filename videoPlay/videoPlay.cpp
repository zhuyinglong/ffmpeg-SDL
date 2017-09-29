#include <iostream>
#include "ffmpegsdl.h"
using namespace std;

int main()
{
	// Register all formats and codecs
	av_register_all();

	// Initalizing to NULL prevents segfaults!
	AVFormatContext   *pFormatCtx = NULL;
	// Open video file
	if (avformat_open_input(&pFormatCtx, "..\\test.mp4", NULL, NULL) != 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		return -1; // Couldn't open file
	}
	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, "1=========================>test.mp4", 0);
	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL)<0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return -1; // Couldn't find stream information
	}
	// 对比上次的Dump可以看出通过avformat_find_stream_info函数往AVFormatContext中填充了流的信息
	av_dump_format(pFormatCtx, 0, "2=========================>test.mp4", 0);

	AVCodec* pDec = NULL;
	/* select the video stream and find the decoder*/
	int video_stream_index = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pDec, 0);
	if (video_stream_index < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		return -1;
	}

	//Note that we must not use the AVCodecContext from the video stream directly! 
	//So we have to use avcodec_copy_context() to copy the context to a new location (after allocating memory for it, of course).	
	/* create decoding context */
	AVCodecContext* pDecCtx = avcodec_alloc_context3(pDec);
	if (!pDecCtx)
	{
		return AVERROR(ENOMEM);
	}

	//Fill the codec context based on the values from the supplied codec
	avcodec_parameters_to_context(pDecCtx, pFormatCtx->streams[video_stream_index]->codecpar);

	//Initialize the AVCodecContext to use the given AVCodec.
	if (avcodec_open2(pDecCtx, pDec, NULL) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
		return -1;
	}


	AVPacket* pPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
	//Allocate an AVFrame and set its fields to default values.
	AVFrame *pFrame = av_frame_alloc();
	AVFrame *pFrameYUV = av_frame_alloc();

	// Determine required buffer size and allocate buffer
	int byteSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P,
		pDecCtx->width,
		pDecCtx->height,
		1
	);
	uint8_t* buffer = (uint8_t *)av_malloc(byteSize);

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize,
		buffer, AV_PIX_FMT_YUV420P,
		pDecCtx->width,
		pDecCtx->height,
		1
	);

	int screen_w = pDecCtx->width / 2;
	int screen_h = pDecCtx->height / 2;
	// initialize SWS context for software scaling
	SwsContext *pSws_ctx = sws_getContext(pDecCtx->width,
		pDecCtx->height,
		pDecCtx->pix_fmt,
		screen_w,
		screen_h,
		AV_PIX_FMT_YUV420P,
		SWS_BICUBIC,
		NULL,
		NULL,
		NULL
	);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	//SDL 2.0 Support for multiple windows  
	SDL_Window* screen = SDL_CreateWindow("SDL",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h,
		SDL_WINDOW_OPENGL
	);

	if (!screen)
	{
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}

	SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	//IYUV: Y + U + V  (3 planes)  
	//YV12: Y + V + U  (3 planes)  
	SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer,
		SDL_PIXELFORMAT_IYUV,
		SDL_TEXTUREACCESS_STREAMING,
		screen_w,
		screen_h
	);
	SDL_Rect sdlRect;
	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	int ret = -1;
	int count = 0;

	SDL_Event event;

	while (1)
	{
		/**
		*Technically a packet can contain partial frames or other bits of data,
		*but ffmpeg's parser ensures that the packets we get contain either complete or multiple frames.
		*/
		ret = av_read_frame(pFormatCtx, pPacket);
		if (ret < 0)
		{
			break;
		}

		if (pPacket->stream_index == video_stream_index)
		{
			//Supply raw packet data as input to a decoder.
			ret = avcodec_send_packet(pDecCtx, pPacket);
			if (ret < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
				break;
			}

			while (ret >= 0)
			{
				//Return decoded output data from a decoder.
				ret = avcodec_receive_frame(pDecCtx, pFrame);//一个包可能有多个帧
				if (ret >= 0)
				{
					// Convert the image from its native format to YUV
					sws_scale(pSws_ctx,
						(const uint8_t* const *)pFrame->data,
						pFrame->linesize,
						0,
						pDecCtx->height,
						pFrameYUV->data,
						pFrameYUV->linesize
					);

				}
				else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					break;
				}
				else
				{
					av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
					goto end;
				}

				SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
					pFrameYUV->data[0], pFrameYUV->linesize[0],
					pFrameYUV->data[1], pFrameYUV->linesize[1],
					pFrameYUV->data[2], pFrameYUV->linesize[2]
				);
				SDL_RenderClear(sdlRenderer);
				SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
				SDL_RenderPresent(sdlRenderer);
				//SDL End-----------------------  
			}
		}
		// Free the packet that was allocated by av_read_frame
		av_packet_unref(pPacket);
		SDL_PollEvent(&event);
		switch (event.type)
		{
		case SDL_QUIT:
			SDL_Quit();
			break;
		default:
			break;
		}
	}
end:
	//Free the swscaler context swsContext
	sws_freeContext(pSws_ctx);

	//Free the codec context and everything associated with it and write NULL to the provided pointer.
	avcodec_free_context(&pDecCtx);

	// Close the video file
	avformat_close_input(&pFormatCtx);

	//Free the frame and any dynamically allocated objects in it
	av_frame_free(&pFrame);
	av_frame_free(&pFrameYUV);

	// Free the RGB image
	av_free(buffer);
	return 0;
}