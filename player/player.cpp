#include <iostream>
#include "ffmpegsdl.h"
using namespace std;

#include "PacketQueue.h"
#include "Audio.h"
#include "Media.h"
#include "VideoDisplay.h"

bool quit = false;

int main()
{
	av_register_all();

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);

	char* filename = "..\\test.mp4";
	MediaState media(filename);

	if (media.openInput())
		SDL_CreateThread(decode_thread, "", &media); // 创建解码线程，读取packet到队列中缓存

	media.audio->audio_play(); // create audio thread

	media.video->video_play(&media); // create video thread

	AVStream *audio_stream = media.audio->stream;
	AVStream *video_stream = media.video->stream;

	double audio_duration = audio_stream->duration * av_q2d(audio_stream->time_base);
	double video_duration = video_stream->duration * av_q2d(video_stream->time_base);

	cout << "audio时长：" << audio_duration << endl;
	cout << "video时长：" << video_duration << endl;

	SDL_Event event;
	while (true) // SDL event loop
	{
		if (quit == 1)
		{ 
			SDL_Quit();
			exit(0);
		}
		SDL_WaitEvent(&event);
		switch (event.type)
		{
		case FF_QUIT_EVENT:
		case SDL_QUIT:
			quit = 1;
			SDL_Quit();
			exit(0);
		case FF_REFRESH_EVENT:
			video_refresh_timer(&media);
			break;
		default:
			break;
		}
	}
	return 0;
}