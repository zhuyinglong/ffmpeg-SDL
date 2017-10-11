#include <iostream>
#include <string>
#include <sstream> //for stringstream
#include <fstream> //for file 
#include "ffmpegsdl.h"

using namespace std;

//将帧保存为PPM图像
void SaveFrame2PPM(AVFrame *pFrame, int width, int height, int iFrame);

//int转string
string num2string(int& num);


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

	AVCodec           *pDec = NULL;
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
	AVCodecContext    *pDecCtx = avcodec_alloc_context3(pDec);
	if (!pDecCtx)
	{
		return AVERROR(ENOMEM);
	}

	//Fill the codec context based on the values from the supplied codec
	avcodec_parameters_to_context(pDecCtx, pFormatCtx->streams[video_stream_index]->codecpar);

	//Initialize the AVCodecContext to use the given AVCodec.
	if (avcodec_open2(pDecCtx, pDec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
		return -1;
	}


	AVPacket* packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	av_init_packet(packet);
	//Allocate an AVFrame and set its fields to default values.
	AVFrame *pFrame = av_frame_alloc();
	AVFrame *pFrameRGB = av_frame_alloc();

	// Determine required buffer size and allocate buffer
	int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pDecCtx->width,pDecCtx->height,1);
	uint8_t* buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize,
		buffer,AV_PIX_FMT_RGB24,
		pDecCtx->width,
		pDecCtx->height,
		1
	);
	// initialize SWS context for software scaling
	SwsContext *sws_ctx = sws_getContext(pDecCtx->width,
		pDecCtx->height,
		pDecCtx->pix_fmt,
		pDecCtx->width,
		pDecCtx->height,
		AV_PIX_FMT_RGB24,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
	);


	int ret = -1;
	int count = 0;
	while (1) 
	{
		/**
		 *Technically a packet can contain partial frames or other bits of data, 
		 *but ffmpeg's parser ensures that the packets we get contain either complete or multiple frames.
		 */
		ret = av_read_frame(pFormatCtx, packet);
		if (ret < 0)
		{
			break;
		}

		if (packet->stream_index == video_stream_index) 
		{
			//Supply raw packet data as input to a decoder.
			ret = avcodec_send_packet(pDecCtx, packet);
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
					// Convert the image from its native format to RGB
					sws_scale(sws_ctx, (const uint8_t* const *)pFrame->data,
						pFrame->linesize, 0, pDecCtx->height,
						pFrameRGB->data, pFrameRGB->linesize);
					if(count < 5 )
					{
						count++;
						SaveFrame2PPM(pFrameRGB, pDecCtx->width, pDecCtx->height, count);
					}
						
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
				
			}
		}
		// Wipe the packet.		
		av_packet_unref(packet);
	}
end:
	// Free the packet that was allocated by av_read_frame
	av_packet_free(&packet);
	//Free the swscaler context swsContext
	sws_freeContext(sws_ctx);
	//Free the codec context and everything associated with it and write NULL to the provided pointer.
	avcodec_free_context(&pDecCtx);
	// Close the video file
	avformat_close_input(&pFormatCtx);
	//Free the frame and any dynamically allocated objects in it
	av_frame_free(&pFrame);
	av_frame_free(&pFrameRGB);
	// Free the RGB image
	av_free(buffer);
	return 0;
}

void SaveFrame2PPM(AVFrame *pFrame, int width, int height, int iFrame)
{
	string strFile = "frame" + num2string(iFrame)+ ".ppm";
	//采用追加模式，以二进制写入到文件  
	ofstream fout(strFile.c_str(), ios_base::out | ios_base::binary);
	if (!fout.is_open())
	{
		cerr << "Can't open " << strFile << endl;
		exit(EXIT_FAILURE);
	}
	
	// Write PPM header
	fout << "P6" << endl;
	fout << width << " " << height << endl;
	fout << "255" << endl;
	

	// Write pixel data
	for (int y = 0; y<height; y++)
	{
		fout.write((const char*)(pFrame->data[0] + y*pFrame->linesize[0]), width * 3);
	}
	fout.close();
	fout.clear();
	cout << strFile << " write!"<<endl;
	
}

//ostringstream对象用来进行格式化的输出，常用于将各种类型转换为string类型
//ostringstream只支持<<操作符
string num2string(int& num)
{
	ostringstream oss;  //创建一个格式化输出流
	oss << num;             //把值传递如流中
	return oss.str();
}