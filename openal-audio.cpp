#include <LepraAssert.h>
#include <DiskFile.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

using namespace Lepra;

#define NUM_BUFFERS 15


#ifdef LEPRA_WINDOWS
extern "C" ALC_API void ALC_APIENTRY alc_init(void);	// Not intended for this type of use, but LGPL OpenAL can't load dsound.dll from DllMain.
#define winalcinit()	alc_init()
#else
#define winalcinit()
#endif
#define AL_CHECK()	deb_assert(alGetError() == AL_NO_ERROR)


extern float gVolume;
DiskFile cache_file;
int track_rate = 0;
int track_channels = 1;


static void error_exit(const char *msg)
{
	puts(msg);
	exit(1);
}

void WavCreateHeader()
{
	cache_file.WriteRaw("RIFF", 4);
	cache_file.Write((int32)0);	// Header 1 size.
	cache_file.WriteRaw("WAVE", 4);
	cache_file.WriteRaw("fmt ", 4);
	cache_file.Write((int32)16);	// Rest of header 2 size.
	cache_file.Write((int16)1);	// PCM
	cache_file.Write((int16)track_channels);
	cache_file.Write(track_rate);
	cache_file.Write((int32)(track_rate*track_channels*sizeof(short)));
	cache_file.Write((int16)(track_channels*sizeof(short)));
	cache_file.Write((int16)(sizeof(short)*8));
	cache_file.WriteRaw("data", 4);
	cache_file.Write((int32)0);	// Data size.
}

void WavCompleteHeader()
{
	const int32 complete_size = (int32)cache_file.Tell();
	int32 data_size = complete_size - 44;
	data_size -= data_size % (track_channels*sizeof(short));
	cache_file.SeekSet(4);
	cache_file.Write(36+data_size);
	cache_file.SeekSet(40);
	cache_file.Write(data_size);
}

static audio_fifo_data_t * audio_get_cache(audio_fifo_t *af)
{
	for (;;) {
		audio_fifo_data_t *afd = audio_get(af);
		if (!afd->fname) {
			if (track_rate == 0) {
				track_rate = afd->rate;
				track_channels = afd->channels;
				WavCreateHeader();
			}
			if (cache_file.IsOpen()) {
				cache_file.WriteData(afd->samples, afd->nsamples * afd->channels * sizeof(short));
			}
			return afd;
		} else {
			if (strcmp("<", afd->fname) == 0) {
				const str path_name = cache_file.GetFullName();
				cache_file.Close();
				if (!path_name.empty()) {
					DiskFile::Delete(path_name);
				}
				track_rate = 0;
			} else if (strcmp(">", afd->fname) == 0) {
				if (cache_file.IsOpen()) {
					WavCompleteHeader();
					str fname = cache_file.GetFullName();
					cache_file.Close();
					DiskFile::Rename(fname, strutil::ReplaceAll(fname, _T(".wavy"), _T(".wav")));
				}
				track_rate = 0;
			} else {
				cache_file.Close();
				track_rate = 0;
				str fname = strutil::Encode(afd->fname);
				fname = strutil::ReplaceAll(fname, _T("/"), _T("_"));
				fname = strutil::ReplaceAll(fname, _T("\\"), _T("_"));
				fname = strutil::ReplaceAll(fname, _T(":"), _T("_"));
				fname = strutil::ReplaceAll(fname, _T("cache_"), _T("cache/"));
				cache_file.Open(fname, DiskFile::MODE_WRITE, true, Endian::TYPE_LITTLE_ENDIAN);
			}
			free(afd->fname);
			free(afd);
		}
	}
}

static int queue_buffer(ALuint source, audio_fifo_t *af, ALuint buffer)
{
	audio_fifo_data_t *afd = audio_get_cache(af);
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
	const float lPos[3] = {0, 0, 0};
	const float lVel[3] = {0, 0, 0};
	const float lDirection[3+3] = {0, 1, 0, 0, 0, 1};	// Forward along Y, up along Z.
	::alListenerfv(AL_POSITION, lPos);
	AL_CHECK();
	::alListenerfv(AL_VELOCITY, lVel);
	AL_CHECK();
	::alListenerfv(AL_ORIENTATION, lDirection);
	AL_CHECK();
	alDistanceModel(AL_NONE);
	AL_CHECK();
	alDopplerFactor(0);
	AL_CHECK();
	alGenBuffers((ALsizei)NUM_BUFFERS, buffers);
	AL_CHECK();
	alGenSources(1, &source);
	AL_CHECK();
	alSourcef(source, AL_GAIN, 1.0);
	AL_CHECK();
	alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
	AL_CHECK();

	/* First prebuffer some audio */
	for (int i = 0; i < NUM_BUFFERS; ++i) {
		queue_buffer(source, af, buffers[i]);
	}
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
			afd = audio_get_cache(af);
			alGetBufferi(buffers[frame % NUM_BUFFERS], AL_FREQUENCY, &rate);
			AL_CHECK();
			alGetBufferi(buffers[frame % NUM_BUFFERS], AL_CHANNELS, &channels);
			AL_CHECK();
			if (afd->rate != rate || afd->channels != channels) {
				printf("rate or channel count changed, resetting\n");
				free(afd);
				break;
			}
			/* Remove old audio from the queue.. */
			alSourceUnqueueBuffers(source, 1, &buffers[frame % NUM_BUFFERS]);
			AL_CHECK();
			alBufferData(buffers[frame % NUM_BUFFERS], 
						 afd->channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, 
						 afd->samples, 
						 afd->nsamples * afd->channels * sizeof(short), 
						 afd->rate);
			AL_CHECK();
			free(afd);
			alSourceQueueBuffers(source, 1, &buffers[frame % NUM_BUFFERS]);
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
		for (int i = 1; i < NUM_BUFFERS; ++i) {
			queue_buffer(source, af, buffers[i]);
		}
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
