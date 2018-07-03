/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "omni_utils.h"

/* Flagging utils */

void block_set_flags(OmniBlock *block, OmniBlockStatusFlags flags)
{
	OmniSample *sample = block->parent;

	if (flags & OMNI_BLOCK_STATUS_CURRENT) {
		flags |= OMNI_BLOCK_STATUS_VALID;

		if (!(block->sflags & OMNI_BLOCK_STATUS_CURRENT)) {
			sample->num_blocks_outdated--;
		}
	}

	if (flags & OMNI_BLOCK_STATUS_VALID) {
		if (!(block->sflags & OMNI_BLOCK_STATUS_VALID)) {
			sample->num_blocks_invalid--;
		}
	}

	block->sflags |= flags;
}

void block_unset_flags(OmniBlock *block, OmniBlockStatusFlags flags)
{
	OmniSample *sample = block->parent;
	OmniSampleStatusFlags sample_flags = 0;

	if (flags & OMNI_BLOCK_STATUS_VALID) {
		flags |= OMNI_BLOCK_STATUS_CURRENT;
		sample_flags |= OMNI_SAMPLE_STATUS_VALID;

		if (block->sflags & OMNI_BLOCK_STATUS_VALID) {
			sample->num_blocks_invalid++;
		}
	}

	if (flags & OMNI_BLOCK_STATUS_CURRENT) {
		sample_flags |= OMNI_SAMPLE_STATUS_CURRENT;

		if (block->sflags & OMNI_BLOCK_STATUS_CURRENT) {
			sample->num_blocks_outdated++;
		}
	}

	block->sflags &= ~flags;

	sample_unset_flags(sample, sample_flags);
}

void meta_set_flags(OmniSample *sample, OmniBlockStatusFlags flags)
{
	if (flags & OMNI_BLOCK_STATUS_CURRENT) {
		flags |= OMNI_BLOCK_STATUS_VALID;
	}

	sample->meta.sflags |= flags;
}

void meta_unset_flags(OmniSample *sample, OmniBlockStatusFlags flags)
{
	OmniSampleStatusFlags sample_flags = 0;

	if (flags & OMNI_BLOCK_STATUS_VALID) {
		flags |= OMNI_BLOCK_STATUS_CURRENT;
		sample_flags |= OMNI_SAMPLE_STATUS_VALID;
	}

	if (flags & OMNI_BLOCK_STATUS_CURRENT) {
		sample_flags |= OMNI_SAMPLE_STATUS_CURRENT;
	}

	sample->meta.sflags &= ~flags;

	sample_unset_flags(sample, sample_flags);
}

void sample_set_flags(OmniSample *sample, OmniSampleStatusFlags flags)
{
	if (flags & OMNI_SAMPLE_STATUS_CURRENT) {
		flags |= OMNI_SAMPLE_STATUS_VALID;
	}

	if (flags & (OMNI_SAMPLE_STATUS_VALID | OMNI_SAMPLE_STATUS_SKIP)) {
		flags |= OMNI_SAMPLE_STATUS_INITED;
	}

	sample->sflags |= flags;
}

void sample_unset_flags(OmniSample *sample, OmniSampleStatusFlags flags)
{
	if (flags & OMNI_SAMPLE_STATUS_INITED) {
		flags |= OMNI_SAMPLE_STATUS_VALID;
	}

	if (flags & OMNI_SAMPLE_STATUS_VALID) {
		flags |= OMNI_SAMPLE_STATUS_CURRENT;
	}

	sample->sflags &= ~flags;
}

void cache_set_flags(OmniCache *cache, OmniCacheStatusFlags flags)
{
	if (flags & OMNICACHE_STATUS_CURRENT) {
		flags |= OMNICACHE_STATUS_VALID;
	}

	if (flags & OMNICACHE_STATUS_VALID) {
		flags |= OMNICACHE_STATUS_INITED;
	}

	cache->sflags |= flags;
}

void cache_unset_flags(OmniCache *cache, OmniCacheStatusFlags flags)
{
	if (flags & OMNICACHE_STATUS_INITED) {
		flags |= OMNICACHE_STATUS_VALID;
	}

	if (flags & OMNICACHE_STATUS_VALID) {
		flags |= OMNICACHE_STATUS_CURRENT;
	}

	cache->sflags &= ~flags;
}

/* Sample utils */

sample_time gen_sample_time(OmniCache *cache, float_or_uint time)
{
	sample_time result;

	assert(cache->ttype == time.isf);

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

		if (IS_ROOT(curr)) {
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

			block_set_flags(block, OMNI_BLOCK_STATUS_INITED);
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
