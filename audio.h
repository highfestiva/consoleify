#pragma once

#include <LepraOS.h>
#include <FastLock.h>
#include <stdint.h>
#include <list>

#pragma warning(disable: 4200)


using namespace Lepra;


struct audio_fifo_data_t {
	int channels;
	int rate;
	int nsamples;
	int16_t samples[];
};

struct audio_fifo_t {
	std::list<audio_fifo_data_t*> q;
	int qlen;
	FastLock mutex;
	FastCondition* cond;

	audio_fifo_t();
	virtual ~audio_fifo_t();
};


extern void audio_init(audio_fifo_t *af);
extern void audio_fifo_flush(audio_fifo_t *af);
audio_fifo_data_t* audio_get(audio_fifo_t *af);
