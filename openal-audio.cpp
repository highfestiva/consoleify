#include <LepraAssert.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

#define NUM_BUFFERS 3


#ifdef LEPRA_WINDOWS
extern "C" ALC_API void ALC_APIENTRY alc_init(void);	// Not intended for this type of use, but LGPL OpenAL can't load dsound.dll from DllMain.
#define winalcinit()	alc_init()
#else
#define winalcinit()
#endif
#define AL_CHECK()	deb_assert(alGetError() == AL_NO_ERROR)


extern float gVolume;


static void error_exit(const char *msg)
{
	puts(msg);
	exit(1);
}

static int queue_buffer(ALuint source, audio_fifo_t *af, ALuint buffer)
{
	audio_fifo_data_t *afd = audio_get(af);
	alBufferData(buffer, 
		afd->channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, 
		afd->samples, 
		afd->nsamples * afd->channels * sizeof(short), 
		afd->rate);
	AL_CHECK();
	alSourceQueueBuffers(source, 1, &buffer);
	AL_CHECK();
	free(afd);
	return 1;
}

static void audio_start(void *aux)
{
	winalcinit();	// Windows-specific.
	audio_fifo_t* af = (audio_fifo_t*)aux;
	audio_fifo_data_t *afd;
	unsigned int frame = 0;
	ALCdevice *device = NULL;
	ALCcontext *context = NULL;
	ALuint buffers[NUM_BUFFERS];
	ALuint source;
	ALint processed = 0;
	ALenum error;
	ALint rate;
	ALint channels;
	device = alcOpenDevice(NULL); /* Use the default device */
	if (!device) error_exit("failed to open device");
	const int attributes[3] = { ALC_FREQUENCY, 44100, 0 };
	context = alcCreateContext(device, attributes);
	alcMakeContextCurrent(context);
	AL_CHECK();
	alListenerf(AL_GAIN, gVolume);
	AL_CHECK();
	//alDistanceModel(AL_NONE);
	AL_CHECK();
	const float lPos[3] = {0, 0, 0};
	const float lVel[3] = {0, 0, 0};
	const float lDirection[3+3] = {0, 1, 0, 0, 0, 1};	// Forward along Y, up along Z.
	::alListenerfv(AL_POSITION, lPos);
	AL_CHECK();
	::alListenerfv(AL_VELOCITY, lVel);
	AL_CHECK();
	::alListenerfv(AL_ORIENTATION, lDirection);
	AL_CHECK();
	alGenBuffers((ALsizei)NUM_BUFFERS, buffers);
	AL_CHECK();
	alGenSources(1, &source);
	AL_CHECK();
	alSourcef(source, AL_GAIN, 1.0);
	AL_CHECK();

	/* First prebuffer some audio */
	queue_buffer(source, af, buffers[0]);
	queue_buffer(source, af, buffers[1]);
	queue_buffer(source, af, buffers[2]);
	for (;;) {
		alSourcePlay(source);
		AL_CHECK();
		for (;;) {
			ALenum state;
			alGetSourcei(source, AL_SOURCE_STATE, &state);
			AL_CHECK();
			if (state == AL_STOPPED)
			{
				alSourcePlay(source);
				AL_CHECK();
			}
			// Wait for some audio to play
			do {
				alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
				AL_CHECK();
				Thread::Sleep(0.01);
			} while (processed == 0);
			
			/* and queue some more audio */
			afd = audio_get(af);
			alGetBufferi(buffers[frame % 3], AL_FREQUENCY, &rate);
			AL_CHECK();
			alGetBufferi(buffers[frame % 3], AL_CHANNELS, &channels);
			AL_CHECK();
			if (afd->rate != rate || afd->channels != channels) {
				printf("rate or channel count changed, resetting\n");
				free(afd);
				break;
			}
			/* Remove old audio from the queue.. */
			alSourceUnqueueBuffers(source, 1, &buffers[frame % 3]);
			AL_CHECK();
			alBufferData(buffers[frame % 3], 
						 afd->channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, 
						 afd->samples, 
						 afd->nsamples * afd->channels * sizeof(short), 
						 afd->rate);
			AL_CHECK();
			free(afd);
			alSourceQueueBuffers(source, 1, &buffers[frame % 3]);
			AL_CHECK();
			
			if ((error = alcGetError(device)) != AL_NO_ERROR) {
				printf("openal al error: %d\n", error);
				exit(1);
			}
			frame++;
		}
		/* Format or rate changed, so we need to reset all buffers */
		alSourcei(source, AL_BUFFER, 0);
		alSourceStop(source);

		/* Make sure we don't lose the audio packet that caused the change */
		alBufferData(buffers[0], 
					 afd->channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, 
					 afd->samples, 
					 afd->nsamples * afd->channels * sizeof(short), 
					 afd->rate);

		alSourceQueueBuffers(source, 1, &buffers[0]);
		queue_buffer(source, af, buffers[1]);
		queue_buffer(source, af, buffers[2]);
		frame = 0;
	}
}

void audio_init(audio_fifo_t *af)
{
	af->qlen = 0;
	af->cond = new FastCondition(&af->mutex);
	StaticThread* player = new StaticThread("Player");
	player->RequestSelfDestruct();
	player->Start(audio_start, af);
}
