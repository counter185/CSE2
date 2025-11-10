// Released under the MIT licence.
// See LICENCE.txt for details.

#include "Backend.h"

#include <mutex>
#include <thread>
#include <atomic>
#include <stddef.h>
#include <string.h>
#include <string>

#ifdef __PS2__
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include "../../Misc.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void (*parent_callback)(long *stream, size_t frames_total) = NULL;

static SDL_AudioDeviceID device_id;

static short* asyncOrganyaStream = NULL;
static unsigned int organyaStreamSize = 0;
std::atomic<bool> dataReady = false;

void OrganyaThread() {
	while (true) {
		if (parent_callback != NULL) {
			if (!dataReady) {
				memset(asyncOrganyaStream, 0, organyaStreamSize);
				const size_t frames_total = organyaStreamSize / sizeof(short) / 2;
				size_t frames_done = 0;

				int orgPos = 0;
				while (frames_done < frames_total) {
					size_t subframes = MIN(0x400, frames_total - frames_done);

					long mix_buffer[0x400 * 2];
					memset(mix_buffer, 0, subframes * sizeof(long) * 2);

					parent_callback(mix_buffer, subframes);

					for (size_t i = 0; i < subframes * 2; ++i)
					{
						if (mix_buffer[i] > 0x7FFF)
							asyncOrganyaStream[orgPos++] = 0x7FFF;
						else if (mix_buffer[i] < -0x7FFF)
							asyncOrganyaStream[orgPos++] = -0x7FFF;
						else
							asyncOrganyaStream[orgPos++] = mix_buffer[i];
					}

					frames_done += subframes;
				}
				dataReady = true;
			}
		}
	}
}

static void Callback(void* user_data, Uint8* stream_uint8, int len)
{
	while (!dataReady) {
	}
	memcpy(stream_uint8, asyncOrganyaStream, len);
	dataReady = false;
}

unsigned long SoftwareMixerBackend_Init(void (*callback)(long *stream, size_t frames_total))
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		std::string errorMessage = std::string("'SDL_InitSubSystem(SDL_INIT_AUDIO)' failed: ") + SDL_GetError();
		Backend_ShowMessageBox("Fatal error (SDL2 audio backend)", errorMessage.c_str());
		return 0;
	}

	Backend_PrintInfo("Available SDL audio drivers:");

	for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i)
		Backend_PrintInfo("%s", SDL_GetAudioDriver(i));

	SDL_AudioSpec specification;
	specification.freq = 48000;
	specification.format = AUDIO_S16SYS;
	specification.channels = 2;
	specification.samples = 0x200;	// Roughly 10 milliseconds for 48000Hz
	specification.callback = Callback;
	specification.userdata = NULL;

	organyaStreamSize = specification.samples * specification.channels * sizeof(short);
	asyncOrganyaStream = (short*)malloc(organyaStreamSize);

	SDL_AudioSpec obtained_specification;
	device_id = SDL_OpenAudioDevice(NULL, 0, &specification, &obtained_specification, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (device_id == 0)
	{
		std::string error_message = std::string("'SDL_OpenAudioDevice' failed: ") + SDL_GetError();
		Backend_ShowMessageBox("Fatal error (SDL2 audio backend)", error_message.c_str());
		return 0;
	}

	Backend_PrintInfo("Selected SDL audio driver: %s", SDL_GetCurrentAudioDriver());

	parent_callback = callback;

	std::thread organyaThread(OrganyaThread);
	organyaThread.detach();

	return obtained_specification.freq;
}

void SoftwareMixerBackend_Deinit(void)
{
	SDL_CloseAudioDevice(device_id);

	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool SoftwareMixerBackend_Start(void)
{
	SDL_PauseAudioDevice(device_id, 0);

	return true;
}

std::mutex orgMutex;
std::mutex mixerMutex;

void SoftwareMixerBackend_LockMixerMutex(void)
{
	mixerMutex.lock();
}

void SoftwareMixerBackend_UnlockMixerMutex(void)
{
	mixerMutex.unlock();
}

void SoftwareMixerBackend_LockOrganyaMutex(void)
{
	orgMutex.lock();
}

void SoftwareMixerBackend_UnlockOrganyaMutex(void)
{
	orgMutex.unlock();
}
