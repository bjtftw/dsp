/*
 * This file is part of dsp.
 *
 * Copyright (c) 2013-2024 Michael Barbour <barbour.michael.0@gmail.com>
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

#include "sampleconv.h"

void write_buf_u8(sample_t *in, void *out, ssize_t s)
{
	uint8_t *outn = (uint8_t *) out;
	ssize_t p = -1;
	while (++p < s)
		outn[p] = SAMPLE_TO_U8(in[p]);
}

void read_buf_u8(void *in, sample_t *out, ssize_t s)
{
	uint8_t *inn = (uint8_t *) in;
	while (s-- > 0)
		out[s] = U8_TO_SAMPLE(inn[s]);
}

void write_buf_s8(sample_t *in, void *out, ssize_t s)
{
	int8_t *outn = (int8_t *) out;
	ssize_t p = -1;
	while (++p < s)
		outn[p] = SAMPLE_TO_S8(in[p]);
}

void read_buf_s8(void *in, sample_t *out, ssize_t s)
{
	int8_t *inn = (int8_t *) in;
	while (s-- > 0)
		out[s] = S8_TO_SAMPLE(inn[s]);
}

void write_buf_s16(sample_t *in, void *out, ssize_t s)
{
	int16_t *outn = (int16_t *) out;
	ssize_t p = -1;
	while (++p < s)
		outn[p] = SAMPLE_TO_S16(in[p]);
}

void read_buf_s16(void *in, sample_t *out, ssize_t s)
{
	int16_t *inn = (int16_t *) in;
	while (s-- > 0)
		out[s] = S16_TO_SAMPLE(inn[s]);
}

void write_buf_s24(sample_t *in, void *out, ssize_t s)
{
	int32_t *outn = (int32_t *) out;
	ssize_t p = -1;
	while (++p < s)
		outn[p] = SAMPLE_TO_S24(in[p]);
}

void read_buf_s24(void *in, sample_t *out, ssize_t s)
{
	int32_t *inn = (int32_t *) in;
	while (s-- > 0)
		out[s] = S24_TO_SAMPLE(inn[s]);
}

void write_buf_s32(sample_t *in, void *out, ssize_t s)
{
	int32_t *outn = (int32_t *) out;
	ssize_t p = -1;
	while (++p < s)
		outn[p] = SAMPLE_TO_S32(in[p]);
}

void read_buf_s32(void *in, sample_t *out, ssize_t s)
{
	int32_t *inn = (int32_t *) in;
	while (s-- > 0)
		out[s] = S32_TO_SAMPLE(inn[s]);
}

void write_buf_s24_3(sample_t *in, void *out, ssize_t s)
{
	int32_t v;
	uint8_t *outn = (uint8_t *) out;
	ssize_t p = -1;
	while (++p < s) {
		v = SAMPLE_TO_S24(in[p]);
		outn[p*3 + 0] = (v >> 0) & 0xff;
		outn[p*3 + 1] = (v >> 8) & 0xff;
		outn[p*3 + 2] = (v >> 16) & 0xff;
	}
}

void read_buf_s24_3(void *in, sample_t *out, ssize_t s)
{
	int32_t v;
	uint8_t *inn = (uint8_t *) in;
	while (s-- > 0) {
		v = (int32_t) (inn[s*3 + 0] & 0xff) << 0;
		v |= (int32_t) (inn[s*3 + 1] & 0xff) << 8;
		v |= (int32_t) (inn[s*3 + 2] & 0xff) << 16;
		out[s] = S24_TO_SAMPLE(v);
	}
}

void write_buf_float(sample_t *in, void *out, ssize_t s)
{
	float *outn = (float *) out;
	ssize_t p = -1;
	while (++p < s)
		outn[p] = SAMPLE_TO_FLOAT(in[p]);
}

void read_buf_float(void *in, sample_t *out, ssize_t s)
{
	float *inn = (float *) in;
	while (s-- > 0)
		out[s] = FLOAT_TO_SAMPLE(inn[s]);
}

void write_buf_double(sample_t *in, void *out, ssize_t s)
{
	double *outn = (double *) out;
	ssize_t p = -1;
	while (++p < s)
		outn[p] = SAMPLE_TO_DOUBLE(in[p]);
}

void read_buf_double(void *in, sample_t *out, ssize_t s)
{
	double *inn = (double *) in;
	while (s-- > 0)
		out[s] = DOUBLE_TO_SAMPLE(inn[s]);
}
