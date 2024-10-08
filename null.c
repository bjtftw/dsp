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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "null.h"

ssize_t null_read(struct codec *c, sample_t *buf, ssize_t frames)
{
	memset(buf, 0, frames * c->channels * sizeof(sample_t));
	return frames;
}

ssize_t null_write(struct codec *c, sample_t *buf, ssize_t frames)
{
	return frames;
}

ssize_t null_seek(struct codec *c, ssize_t pos)
{
	return -1;
}

ssize_t null_delay(struct codec *c)
{
	return 0;
}

void null_drop(struct codec *c)
{
	/* do nothing */
}

void null_pause(struct codec *c, int p)
{
	/* do nothing */
}

void null_destroy(struct codec *c)
{
	/* nothing to clean up */
}

struct codec * null_codec_init(const char *path, const char *type, const char *enc, int fs, int channels, int endian, int mode)
{
	struct codec *c = calloc(1, sizeof(struct codec));
	c->path = "null";
	c->type = type;
	c->enc = "sample_t";
	c->fs = fs;
	c->channels = channels;
	c->prec = 53;
	c->frames = -1;
	c->read = null_read;
	c->write = null_write;
	c->seek = null_seek;
	c->delay = null_delay;
	c->drop = null_drop;
	c->pause = null_pause;
	c->destroy = null_destroy;
	c->data = NULL;

	return c;
}

void null_codec_print_encodings(const char *type)
{
	fprintf(stdout, " sample_t");
}
