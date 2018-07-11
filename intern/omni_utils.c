/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "omni_utils.h"

/* Flagging utils */

void block_set_status(OmniBlock *block, OmniBlockStatusFlags status)
{
	OmniSample *sample = block->parent;

	if (status & OMNI_STATUS_CURRENT) {
		status |= OMNI_STATUS_VALID;

		if (!(block->status & OMNI_STATUS_CURRENT)) {
			sample->num_blocks_outdated--;
		}
	}

	if (status & OMNI_STATUS_VALID) {
		if (!(block->status & OMNI_STATUS_VALID)) {
			sample->num_blocks_invalid--;
		}
	}

	block->status |= status;
}

void block_unset_status(OmniBlock *block, OmniBlockStatusFlags status)
{
	OmniSample *sample = block->parent;

	if (status & OMNI_STATUS_VALID) {
		status |= OMNI_STATUS_CURRENT;

		if (block->status & OMNI_STATUS_VALID) {
			sample->num_blocks_invalid++;
		}
	}

	if (status & OMNI_STATUS_CURRENT) {
		if (block->status & OMNI_STATUS_CURRENT) {
			sample->num_blocks_outdated++;
		}
	}

	block->status &= ~status;
}

void meta_set_status(OmniSample *sample, OmniBlockStatusFlags status)
{
	if (status & OMNI_STATUS_CURRENT) {
		status |= OMNI_STATUS_VALID;
	}

	sample->meta.status |= status;
}

void meta_unset_status(OmniSample *sample, OmniBlockStatusFlags status)
{
	if (status & OMNI_STATUS_VALID) {
		status |= OMNI_STATUS_CURRENT;
	}

	sample->meta.status &= ~status;
}

void sample_set_status(OmniSample *sample, OmniSampleStatusFlags status)
{
	if (status & OMNI_STATUS_CURRENT) {
		status |= OMNI_STATUS_VALID;
	}

	if (status & (OMNI_STATUS_VALID | OMNI_SAMPLE_STATUS_SKIP)) {
		status |= OMNI_STATUS_INITED;
	}

	sample->status |= status;
}

void sample_unset_status(OmniSample *sample, OmniSampleStatusFlags status)
{
	if (status & OMNI_STATUS_INITED) {
		status |= OMNI_STATUS_VALID;
	}

	if (status & OMNI_STATUS_VALID) {
		status |= OMNI_STATUS_CURRENT;
	}

	sample->status &= ~status;
}

void cache_set_status(OmniCache *cache, OmniCacheStatusFlags status)
{
	if (status & OMNI_STATUS_CURRENT) {
		status |= OMNI_STATUS_VALID;
	}

	if (status & OMNI_STATUS_VALID) {
		status |= OMNI_STATUS_INITED;
	}

	cache->status |= status;
}

void cache_unset_status(OmniCache *cache, OmniCacheStatusFlags status)
{
	if (status & OMNI_STATUS_INITED) {
		status |= OMNI_STATUS_VALID;
	}

	if (status & OMNI_STATUS_VALID) {
		status |= OMNI_STATUS_CURRENT;
	}

	cache->status &= ~status;
}

/* Sample utils */

sample_time gen_sample_time(OmniCache *cache, float_or_uint time)
{
	sample_time result = {};

	assert(TTYPE_FLOAT(cache->ttype) == time.isf);

	if (FU_LT(time, cache->tinitial) || FU_GT(time, cache->tfinal)) {
		result.ttype = OMNI_TIME_INVALID;
		return result;
	}

	time = fu_sub(time, cache->tinitial);

	result.ttype = cache->ttype;
	result.index = fu_uint(fu_div(time, cache->tstep));
	result.offset = fu_mod(time, cache->tstep);

	return result;
}

/* Call a function for each sample in the cache, starting from an arbitrary sample.
 * start: sample at which to start iterating.
 * list: function called for all listed samples (non-root).
 * root: function called for all root samples.
 * first: function called for the `start` sample in addition to the `list` or `root` function. */
void samples_iterate(OmniSample *start, iter_callback list,
                     iter_callback root, iter_callback first)
{
	assert(list);

	if (start) {
		OmniCache *cache = start->parent;
		OmniSample *curr = start;
		OmniSample *next = curr->next;
		uint index = curr->tindex;

		if (first) first(curr);

		if (SAMPLE_IS_ROOT(curr)) {
			if (root) root(curr);
		}
		else {
			if (list) list(curr);
		}

		if (list) {
			for (curr = next; curr; curr = next) {
				next = curr->next;

				list(curr);
			}
		}

		for (uint i = index + 1; i < cache->num_samples_array; i++) {
			curr = &cache->samples[i];
			next = curr->next;

			if (root) root(curr);

			if (list) {
				for (curr = next; curr; curr = next) {
					next = curr->next;

					list(curr);
				}
			}
		}
	}
}

OmniSample *sample_prev(OmniSample *sample)
{
	OmniCache *cache = sample->parent;
	OmniSample *prev = &cache->samples[sample->tindex];

	while (prev->next != sample) {
		prev = prev->next;
	}

	return prev;
}

/* Find last sample at certain index */
OmniSample *sample_last(OmniSample *sample)
{
	while (sample->next != NULL) {
		sample = sample->next;
	}

	return sample;
}

void resize_sample_array(OmniCache *cache, uint size)
{
	cache->samples = realloc(cache->samples, sizeof(OmniSample) * size);

	if (size > cache->num_samples_alloc) {
		uint start = cache->num_samples_alloc;
		uint length = size - start;
		memset(&cache->samples[start], 0, sizeof(OmniSample) * length);
	}

	cache->num_samples_alloc = size;
}

void init_sample_blocks(OmniSample *sample)
{
	if (!sample->blocks) {
		OmniCache *cache = sample->parent;

		sample->blocks = calloc(cache->num_blocks, sizeof(OmniBlock));
		sample->num_blocks_invalid = cache->num_blocks;
		sample->num_blocks_outdated = cache->num_blocks;

		for (uint i = 0; i < cache->num_blocks; i++) {
			OmniBlock *block = &sample->blocks[i];

			block->parent = sample;

			block_set_status(block, OMNI_STATUS_INITED);
		}
	}
}

void update_block_parents(OmniCache *cache)
{
	for (uint i = 0; i < cache->num_samples_array; i++) {
		OmniSample *samp = &cache->samples[i];

		do {
			if (samp->blocks) {
				for (uint j = 0; j < cache->num_blocks; j++) {
					samp->blocks[j].parent = samp;
				}
			}

			samp = samp->next;
		} while (samp);
	}
}
