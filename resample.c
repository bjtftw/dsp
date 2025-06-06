/*
 * This file is part of dsp.
 *
 * Copyright (c) 2014-2025 Michael Barbour <barbour.michael.0@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include "resample.h"
#include "util.h"

/* Tunables */
static const double default_bw = 0.95;  /* default bandwidth */
#define SINC_MAX_OVERSAMPLE 2
#define WINDOW_SHAPE        2  /* 1: Blackman; 2: Nuttall, continuous first derivative */

struct resample_state {
	struct {
		int n, d;
	} ratio;
	int sinc_fr_len, tmp_fr_len, in_len, out_len;
	int in_buf_pos, out_buf_pos, drain_pos, drain_frames, out_delay;
	fftw_complex *sinc_fr;
	fftw_complex *tmp_fr, *tmp_fr_2;
	sample_t **input, **output, **overlap;
	fftw_plan *r2c_plan, *c2r_plan;
	int has_output, is_draining;
};

static double window(const double x)
{
#if WINDOW_SHAPE == 1
	/* Blackman (~75dB stopband attenuation) */
	#define M_FACT 6
	if (x >= 1.0 || x <= 0.0) return 0.0;
	return 0.42 - 0.5*cos(2*M_PI*x) + 0.08*cos(4*M_PI*x);
#elif WINDOW_SHAPE == 2
	/* Nuttall window (continuous first derivative) (~112dB stopband attenuation) */
	#define M_FACT 8
	if (x >= 1.0 || x <= 0.0) return 0.0;
	return 0.355768 - 0.487396*cos(2*M_PI*x) + 0.144232*cos(4*M_PI*x) - 0.012604*cos(6*M_PI*x);
#else
	#error "error: illegal WINDOW_SHAPE"
#endif
}

static double norm_sinc(const double x, const double fc)
{
	if (fabs(x) < 1e-9)
		return fc;
	return sin(M_PI*fc*x) / (M_PI*x);
}

sample_t * resample_effect_run(struct effect *e, ssize_t *frames, sample_t *ibuf, sample_t *obuf)
{
	struct resample_state *state = (struct resample_state *) e->data;
	ssize_t iframes = 0, oframes = 0;
	const ssize_t max_oframes = ratio_mult_ceil(*frames, state->ratio.n, state->ratio.d);

	while (iframes < *frames) {
		while (state->in_buf_pos < state->in_len && iframes < *frames) {
			for (int i = 0; i < e->ostream.channels; ++i)
				state->input[i][state->in_buf_pos] = ibuf[iframes * e->ostream.channels + i];
			++iframes;
			++state->in_buf_pos;
		}

		while (state->out_buf_pos < state->out_len && oframes < max_oframes && state->has_output) {
			for (int i = 0; i < e->ostream.channels; ++i)
				obuf[oframes * e->ostream.channels + i] = state->output[i][state->out_buf_pos];
			++oframes;
			++state->out_buf_pos;
		}

		if (state->in_buf_pos == state->in_len && (!state->has_output || state->out_buf_pos == state->out_len)) {
			for (int i = 0; i < e->ostream.channels; ++i) {
				/* FFT(state->input[i]) -> state->tmp_fr */
				fftw_execute(state->r2c_plan[i]);
				memset(state->tmp_fr_2, 0, state->tmp_fr_len * sizeof(fftw_complex));
				/* convolve input with sinc filter */
				state->tmp_fr_2[0] = state->tmp_fr[0] * state->sinc_fr[0];
				for (int k = 1, j = 1, l = 1, d1 = 1, d2 = 1;; ++k) {
					fftw_complex s = (d1 == 1) ? state->tmp_fr[j] : conj(state->tmp_fr[j]);
					state->tmp_fr_2[l] += (d2 == 1) ? s * state->sinc_fr[k] : conj(s * state->sinc_fr[k]);
					if (k + 1 == state->sinc_fr_len) break;
					if (l == state->out_len)
						state->tmp_fr_2[l] += s * state->sinc_fr[k];
					else if (l == 0)
						state->tmp_fr_2[l] += conj(s * state->sinc_fr[k]);
					j += d1;
					l += d2;
					if (j == 0) d1 = 1;
					else if (j == state->in_len) d1 = -1;
					if (l == 0) d2 = 1;
					else if (l == state->out_len) d2 = -1;
				}
				/* IFFT(state->tmp_fr_2) -> state->output[i] */
				fftw_execute(state->c2r_plan[i]);
				/* normalize */
				for (int k = 0; k < state->out_len * 2; ++k)
					state->output[i][k] /= state->in_len * 2;
				/* handle overlap */
				for (int k = 0; k < state->out_len; ++k) {
					state->output[i][k] += state->overlap[i][k];
					state->overlap[i][k] = state->output[i][k + state->out_len];
				}
			}
			state->in_buf_pos = state->out_buf_pos = 0;
			if (state->has_output == 0) {
				state->out_buf_pos = state->out_delay;
				state->has_output = 1;
			}
		}
	}
	*frames = oframes;
	return obuf;
}

ssize_t resample_effect_delay(struct effect *e)
{
	ssize_t frames = 0;
	struct resample_state *state = (struct resample_state *) e->data;
	if (state->has_output) {
		frames += state->out_delay;  /* filter delay */
		frames += state->out_len - state->out_buf_pos;  /* pending output frames */
	}
	frames += ratio_mult_ceil(state->in_buf_pos, state->ratio.n, state->ratio.d);  /* pending input frames */
	return frames;
}

void resample_effect_reset(struct effect *e)
{
	int i;
	struct resample_state *state = (struct resample_state *) e->data;
	state->in_buf_pos = state->out_buf_pos = 0;
	state->has_output = 0;
	for (i = 0; i < e->ostream.channels; ++i)
		memset(state->overlap[i], 0, state->out_len * sizeof(sample_t));
}

sample_t * resample_effect_drain2(struct effect *e, ssize_t *frames, sample_t *buf1, sample_t *buf2)
{
	struct resample_state *state = (struct resample_state *) e->data;
	sample_t *rbuf = buf1;
	if (!state->has_output && state->in_buf_pos == 0)
		*frames = -1;
	else {
		if (!state->is_draining) {
			if (state->has_output) {
				state->drain_frames += state->out_delay;  /* filter delay */
				state->drain_frames += state->out_len - state->out_buf_pos;  /* pending output frames */
			}
			state->drain_frames += ratio_mult_ceil(state->in_buf_pos, state->ratio.n, state->ratio.d);  /* pending input frames */
			state->is_draining = 1;
		}
		if (state->drain_pos < state->drain_frames) {
			memset(buf1, 0, *frames * e->ostream.channels * sizeof(sample_t));
			rbuf = resample_effect_run(e, frames, buf1, buf2);
			state->drain_pos += *frames;
			*frames -= (state->drain_pos > state->drain_frames) ? state->drain_pos - state->drain_frames : 0;
		}
		else
			*frames = -1;
	}
	return rbuf;
}

void resample_effect_destroy(struct effect *e)
{
	int i;
	struct resample_state *state = (struct resample_state *) e->data;
	fftw_free(state->sinc_fr);
	fftw_free(state->tmp_fr);
	fftw_free(state->tmp_fr_2);
	for (i = 0; i < e->ostream.channels; ++i) {
		fftw_free(state->input[i]);
		fftw_free(state->output[i]);
		fftw_free(state->overlap[i]);
		fftw_destroy_plan(state->r2c_plan[i]);
		fftw_destroy_plan(state->c2r_plan[i]);
	}
	free(state->input);
	free(state->output);
	free(state->overlap);
	free(state->r2c_plan);
	free(state->c2r_plan);
	free(state);
}

struct effect * resample_effect_init(const struct effect_info *ei, const struct stream_info *istream, const char *channel_selector, const char *dir, int argc, const char *const *argv)
{
	struct effect *e;
	struct resample_state *state;
	char *endptr;
	int rate;
	double bw = default_bw;
	sample_t *sinc;
	fftw_plan sinc_plan;

	if (argc < 2 || argc > 3) {
		LOG_FMT(LL_ERROR, "%s: usage: %s", argv[0], ei->usage);
		return NULL;
	}
	if (argc == 3) {
		bw = strtod(argv[1], &endptr);
		CHECK_ENDPTR(argv[1], endptr, "bandwidth", return NULL);
		rate = lround(parse_freq(argv[2], &endptr));
		CHECK_ENDPTR(argv[2], endptr, "fs", return NULL);
	}
	else {
		rate = lround(parse_freq(argv[1], &endptr));
		CHECK_ENDPTR(argv[1], endptr, "fs", return NULL);
	}
	CHECK_RANGE(bw >= 0.8 && bw <= 0.999, "bandwidth", return NULL);
	CHECK_RANGE(rate > 0, "rate", return NULL);

	e = calloc(1, sizeof(struct effect));
	if (rate == istream->fs) {
		LOG_FMT(LL_VERBOSE, "%s: info: sample rates match; no proccessing will be done", argv[0]);
		return e;  /* Note: the effect will not be used because run() is unset */
	}
	e->name = ei->name;
	e->istream.fs = istream->fs;
	e->ostream.fs = rate;
	e->istream.channels = e->ostream.channels = istream->channels;
	e->run = resample_effect_run;
	e->delay = resample_effect_delay;
	e->reset = resample_effect_reset;
	e->drain2 = resample_effect_drain2;
	e->destroy = resample_effect_destroy;

	state = calloc(1, sizeof(struct resample_state));
	e->data = state;

	const int max_rate = MAXIMUM(rate, istream->fs);
	const int min_rate = MINIMUM(rate, istream->fs);
	const int gcd = find_gcd(rate, istream->fs);
	state->ratio.n = rate / gcd;
	state->ratio.d = istream->fs / gcd;
	const int max_factor = MAXIMUM(state->ratio.n, state->ratio.d);
	const int min_factor = MINIMUM(state->ratio.n, state->ratio.d);

	/* calulate params for windowed sinc function */
	const double width = (min_rate - min_rate * bw) / 2.0;
	const double fc = (min_rate - width) / max_rate;
	const int m = lround(M_FACT / (width / max_rate));
	const int sinc_os = MINIMUM(min_factor, SINC_MAX_OVERSAMPLE);
	const double fc_os = fc / sinc_os;
	const int m_os = (m + 1) * sinc_os - 1;

	/* determine array lengths */
	const int m_conv = (m + 1) * 2 - 1;  /* after convolving sinc function with itself */
	int len_mult = (m_conv + 1) / max_factor;
	if ((m_conv + 1) % max_factor != 0) len_mult += 1;
	if (len_mult > 16) {  /* 17 is the first slow size */
		const int fast_len_mult = next_fast_fftw_len(len_mult);
		if (fast_len_mult != len_mult
				&& (state->ratio.n <= 16 || state->ratio.d <= 16
					|| next_fast_fftw_len(state->ratio.n) == state->ratio.n
					|| next_fast_fftw_len(state->ratio.d) == state->ratio.d))
			len_mult = fast_len_mult;
	}
	const int sinc_len = max_factor * len_mult * sinc_os;
	state->in_len = state->ratio.d * len_mult;
	state->out_len = state->ratio.n * len_mult;
	state->tmp_fr_len = max_factor * len_mult + 1;
	state->sinc_fr_len = sinc_len + 1;

	/* calculate output delay */
	if (rate == max_rate)
		state->out_delay = m_conv / 2;
	else
		state->out_delay = lround(m_conv / 2 * ((double) state->ratio.n / state->ratio.d));

	/* allocate arrays, construct fftw plans */
	state->input = calloc(e->ostream.channels, sizeof(sample_t *));
	state->output = calloc(e->ostream.channels, sizeof(sample_t *));
	state->overlap = calloc(e->ostream.channels, sizeof(sample_t *));
	state->r2c_plan = calloc(e->ostream.channels, sizeof(fftw_plan));
	state->c2r_plan = calloc(e->ostream.channels, sizeof(fftw_plan));
	state->tmp_fr = fftw_malloc(state->tmp_fr_len * sizeof(fftw_complex));
	state->tmp_fr_2 = fftw_malloc(state->tmp_fr_len * sizeof(fftw_complex));
	sinc = fftw_malloc(sinc_len * 2 * sizeof(sample_t));
	state->sinc_fr = fftw_malloc(state->sinc_fr_len * sizeof(fftw_complex));

	dsp_fftw_acquire();
	const int planner_flags = (dsp_fftw_load_wisdom()) ? FFTW_MEASURE : FFTW_ESTIMATE;
	sinc_plan = fftw_plan_dft_r2c_1d(sinc_len * 2, sinc, state->sinc_fr, FFTW_ESTIMATE);
	for (int i = 0; i < e->ostream.channels; ++i) {
		state->input[i] = fftw_malloc(state->in_len * 2 * sizeof(sample_t));
		state->output[i] = fftw_malloc(state->out_len * 2 * sizeof(sample_t));
		state->overlap[i] = fftw_malloc(state->out_len * sizeof(sample_t));
		state->r2c_plan[i] = fftw_plan_dft_r2c_1d(state->in_len * 2, state->input[i], state->tmp_fr, planner_flags);
		state->c2r_plan[i] = fftw_plan_dft_c2r_1d(state->out_len * 2, state->tmp_fr_2, state->output[i], planner_flags);
		memset(state->input[i], 0, state->in_len * 2 * sizeof(sample_t));
		memset(state->output[i], 0, state->out_len * 2 * sizeof(sample_t));
		memset(state->overlap[i], 0, state->out_len * sizeof(sample_t));
	}
	dsp_fftw_release();
	memset(sinc, 0, sinc_len * 2 * sizeof(sample_t));
	memset(state->sinc_fr, 0, state->sinc_fr_len * sizeof(fftw_complex));
	memset(state->tmp_fr, 0, state->tmp_fr_len * sizeof(fftw_complex));
	memset(state->tmp_fr_2, 0, state->tmp_fr_len * sizeof(fftw_complex));

	/* generate windowed sinc function */
	/* note: all supported windows are zero at endpoints, so skip the first and last indicies */
	for (int i = 1; i < m_os; ++i)
		sinc[i] = norm_sinc((i*2 - m_os)/2.0, fc_os) * window((double) i / m_os);

	fftw_execute(sinc_plan);
	fftw_destroy_plan(sinc_plan);
	fftw_free(sinc);

	/* convolve sinc function with itself (doubles stopband attenuation) */
	for (int i = 0; i < state->sinc_fr_len; ++i)
		state->sinc_fr[i] *= state->sinc_fr[i];

	LOG_FMT(LL_VERBOSE, "%s: info: gcd=%d ratio=%d/%d width=%fHz fc=%f filter_len=%d in_len=%d out_len=%d sinc_oversample=%d",
		argv[0], gcd, state->ratio.n, state->ratio.d, width, fc, m_conv+1, state->in_len, state->out_len, sinc_os);

	return e;
}
