/*
 * Copyright (c) 2010 Spotify Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * Audio helper functions.
 *
 * This file is part of the libspotify examples suite.
 */

#include "audio.h"
#include <stdlib.h>


audio_fifo_t::audio_fifo_t():
	qlen(0),
	cond(0)
{
}

audio_fifo_t::~audio_fifo_t()
{
	qlen = 0;
	delete cond;
}


void audio_put_fname(audio_fifo_t *af, const char* s) {
	audio_fifo_data_t* afd = (audio_fifo_data_t*)malloc(sizeof(*afd));
	afd->fname = (char*)malloc(strlen(s)+1);
	afd->nsamples = 0;
	strcpy(afd->fname, s);
	audio_put(af, afd, 0);
}

void audio_put(audio_fifo_t *af, audio_fifo_data_t* afd, int num_frames)
{
	af->mutex.Acquire();

	af->q.push_back(afd);
	af->qlen += num_frames;

	af->cond->Signal();
	af->mutex.Release();
}

audio_fifo_data_t* audio_get(audio_fifo_t *af)
{
	audio_fifo_data_t *afd;
	af->mutex.Acquire();
  
	while (af->q.empty())
		af->cond->Wait();

	afd = af->q.front();
	af->q.pop_front();
	af->qlen -= afd->nsamples;
  
	af->mutex.Release();
	return afd;
}

bool audio_is_empty(audio_fifo_t *af) {
	af->mutex.Acquire();
	const bool empty = af->q.empty();
	af->mutex.Release();
	return empty;
}

void audio_fifo_flush(audio_fifo_t *af)
{
	audio_fifo_data_t* afd;
	af->mutex.Acquire();
	while (!af->q.empty()) {
		afd = af->q.front();
		af->q.pop_front();
		if (afd->fname) {
			free(afd->fname);
			afd->fname = 0;
		}
		free(afd);
	}
	af->qlen = 0;

	// Delete any track caching in progress.
	audio_put_fname(af, "<");

	af->mutex.Release();
}
