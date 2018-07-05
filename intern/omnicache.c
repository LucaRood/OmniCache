/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "omnicache.h"

#include "omni_utils.h"

#define MIN_SAMPLES 10

static OmniSample *sample_get(OmniCache *cache, sample_time stime, bool create)
{
	OmniSample *sample = NULL;

	if (stime.index >= cache->num_samples_alloc) {
		if (create) {
			uint size = MAX(cache->num_samples_alloc, MIN_SAMPLES);

			if (stime.index >= size) {
				size *= pow_u(2, (int)ceil(log2((double)(stime.index + 1) / (double)size)));

				resize_sample_array(cache, size);

				update_block_parents(cache);
			}
		}
		else {
			return NULL;
		}
	}

	/* Increment array sample count until required sample, initializing all samples along the way. */
	if (cache->num_samples_array <= stime.index) {
		if (create) {
			for (; cache->num_samples_array < stime.index; cache->num_samples_array++) {
				OmniSample *samp = &cache->samples[cache->num_samples_array];

				samp->parent = cache;
				samp->tindex = cache->num_samples_array;
				sample_set_flags(samp, OMNI_SAMPLE_STATUS_SKIP);
			}

			cache->num_samples_array++;
		}
		else {
			return NULL;
		}
	}

	/* Find or add sample. */
	if (FU_FL_EQ(stime.offset, 0.0f)) {
		/* Sample is at time zero (i.e. sits directly in the array). */
		sample = &cache->samples[stime.index];
	}
	else {
		OmniSample *prev = &cache->samples[stime.index];
		OmniSample *next = prev->next;

		while (next && FU_LT(next->toffset, stime.offset)) {
			prev = next;
			next = next->next;
		}

		if (FU_EQ(next->toffset, stime.offset)) {
			/* Sample already exists. */
			sample = next;
		}
		else if (create) {
			/* New sample should be created. */
			sample = calloc(1, sizeof(OmniSample));
			sample->toffset = stime.offset;

			prev->next = sample;
			sample->next = next;
		}
		else {
			return NULL;
		}
	}

	if (create) {
		sample->parent = cache;
		sample->tindex = stime.index;

		init_sample_blocks(sample);

		sample_set_flags(sample, OMNI_SAMPLE_STATUS_INITED);
		sample_unset_flags(sample, OMNI_SAMPLE_STATUS_SKIP);
	}

	return sample;
}

/* Utility to get sample without manually generating `sample_time`. */
static OmniSample *sample_get_from_time(OmniCache *cache, float_or_uint time, bool create)
{
	sample_time stime = gen_sample_time(cache, time);

	return sample_get(cache, stime, create);
}

/* Free all blocks in a sample (also frees metadata) */
static void blocks_free(OmniSample *sample)
{
	OmniCache *cache = sample->parent;

	if (sample->blocks) {
		for (uint i = 0; i < cache->num_blocks; i++) {
			free(sample->blocks[i].data);
		}

		free(sample->blocks);
		sample->blocks = NULL;
	}

	if (sample->meta.data) {
		free(sample->meta.data);
		sample->meta.data = NULL;
	}

	meta_unset_flags(sample, OMNI_BLOCK_STATUS_VALID);
	sample_unset_flags(sample, OMNI_SAMPLE_STATUS_VALID);
}

/* Sample iterator helpers */

static void sample_mark_outdated(OmniSample *sample)
{
	sample_unset_flags(sample, OMNI_SAMPLE_STATUS_CURRENT);
}

static void sample_mark_invalid(OmniSample *sample)
{
	sample_unset_flags(sample, OMNI_SAMPLE_STATUS_VALID);
}

static void sample_clear_ref(OmniSample *sample)
{
	if (!IS_ROOT(sample)) {
		OmniSample *prev = sample_prev(sample);

		prev->next = NULL;
	}
}

static void sample_remove_list(OmniSample *sample)
{
	blocks_free(sample);

	free(sample);
}

static void sample_remove_root(OmniSample *sample)
{
	blocks_free(sample);

	sample_set_flags(sample, OMNI_SAMPLE_STATUS_SKIP);
}

static void sample_remove(OmniSample *sample)
{
	if (sample) {
		if (IS_ROOT(sample)) {
			sample_remove_root(sample);
		}
		else {
			OmniSample *prev = sample_prev(sample);
			prev->next = sample->next;

			sample_remove_list(sample);
		}
	}
}

static void sample_remove_invalid(OmniSample *sample)
{
	if (sample && !(sample->sflags & OMNI_SAMPLE_STATUS_VALID)) {
		sample_remove(sample);
	}
}

static void sample_remove_outdated(OmniSample *sample)
{
	if (sample && (!(sample->sflags & OMNI_SAMPLE_STATUS_VALID) ||
	               !(sample->sflags & OMNI_SAMPLE_STATUS_CURRENT)))
	{
		sample_remove(sample);
	}
}


static void samples_free(OmniCache *cache)
{
	if (cache->samples) {
		samples_iterate(cache->samples, sample_remove_list, blocks_free, NULL);

		free(cache->samples);
		cache->samples = NULL;
	}

	cache->num_samples_alloc = 0;
	cache->num_samples_array = 0;
	cache->num_samples_tot = 0;
}


/* Public API functions */

float_or_uint OMNI_f_to_fu(float val)
{
	float_or_uint fou = {
	    .isf = true,
	    .f = val,
	};

	return fou;
}

float_or_uint OMNI_u_to_fu(uint val)
{
	float_or_uint fou = {
	    .isf = false,
	    .u = val,
	};

	return fou;
}

OmniCache *OMNI_new(const OmniCacheTemplate *cache_temp, const OmniBlockTemplateArray block_temp)
{
	OmniCache *cache = calloc(1, sizeof(OmniCache));

	assert(FU_FL_GT(cache_temp->time_step, 0.0f));
	assert(cache_temp->time_type == cache_temp->time_initial.isf);
	assert(cache_temp->time_type == cache_temp->time_final.isf);
	assert(cache_temp->time_type == cache_temp->time_step.isf);
	assert(FU_LE(cache_temp->time_initial, cache_temp->time_final));

	cache->tinitial = cache_temp->time_initial;
	cache->tinitial = cache_temp->time_final;
	cache->tstep = cache_temp->time_step;

	cache->ttype = cache_temp->time_type;
	cache->flags = cache_temp->flags;
	cache->msize = cache_temp->meta_size;

	cache->meta_gen = cache_temp->meta_gen;

	/* Blocks */
	assert(!(cache_temp->num_blocks != 0 && block_temp == NULL));

	cache->num_blocks = cache_temp->num_blocks;

	if (cache->num_blocks) {
		cache->block_index = calloc(cache->num_blocks, sizeof(OmniBlockInfo));

		for (uint i = 0; i < cache->num_blocks; i++) {
			OmniBlockInfo *b_info = &cache->block_index[i];
			const OmniBlockTemplate *b_temp = &block_temp[i];

			b_info->parent = cache;

			b_info->dtype = b_temp->data_type;
			b_info->flags = b_temp->flags;

			strncpy(b_info->name, b_temp->name, MAX_NAME);

			b_info->dsize = DATA_SIZE(b_temp->data_type, b_temp->data_size);

			assert(b_temp->count);
			assert(b_temp->read);
			assert(b_temp->write);

			b_info->count = b_temp->count;
			b_info->read = b_temp->read;
			b_info->write = b_temp->write;
			b_info->interp = b_temp->interp;
		}
	}

	cache_set_flags(cache, OMNICACHE_STATUS_CURRENT);

	return cache;
}

void OMNI_free(OmniCache *cache)
{
	samples_free(cache);

	free(cache->block_index);
	free(cache);
}

bool OMNI_sample_write(OmniCache *cache, float_or_uint time, void *data)
{
	OmniSample *sample = sample_get_from_time(cache, time, true);

	for (uint i = 0; i < cache->num_blocks; i++) {
		OmniBlockInfo *b_info = &cache->block_index[i];
		OmniBlock *block = &sample->blocks[i];
		OmniData omni_data;
		uint dcount = b_info->count(data);

		if (block->data && block->dcount != dcount) {
			free(block->data);
			block->data = NULL;
		}

		block->dcount = dcount;

		if (!block->data) {
			block->data = malloc(b_info->dsize * block->dcount);
		}

		omni_data.dtype = b_info->dtype;
		omni_data.dsize = b_info->dsize;
		omni_data.dcount = block->dcount;
		omni_data.data = block->data;

		if (b_info->write(&omni_data, data)) {
			block_set_flags(block, OMNI_BLOCK_STATUS_CURRENT);
		}
		else {
			block_unset_flags(block, OMNI_BLOCK_STATUS_VALID);
			sample_unset_flags(sample, OMNI_SAMPLE_STATUS_VALID);

			return false;
		}

		/* Ensure the user did not reallocate the data pointer. */
		assert(omni_data.data == block->data);
	}

	if (cache->meta_gen) {
		if (!sample->meta.data) {
			sample->meta.data = malloc(cache->msize);
		}

		if (cache->meta_gen(data, sample->meta.data)) {
			meta_set_flags(sample, OMNI_BLOCK_STATUS_CURRENT);
		}
		else {
			meta_unset_flags(sample, OMNI_BLOCK_STATUS_VALID);
			sample_unset_flags(sample, OMNI_SAMPLE_STATUS_VALID);

			return false;
		}
	}

	sample_set_flags(sample, OMNI_SAMPLE_STATUS_CURRENT);

	return true;
}

OmniReadResult OMNI_sample_read(OmniCache *cache, float_or_uint time, void *data)
{
	OmniSample *sample = NULL;
	OmniReadResult result = OMNI_READ_EXACT;

	/* TODO: Status flags should be generalized so that IS_VALID and IS_CURRENT can be used here. */
	if (!(cache->sflags & OMNICACHE_STATUS_VALID)) {
		return OMNI_READ_INVALID;
	}

	if (!(cache->sflags & OMNICACHE_STATUS_CURRENT)) {
		result = OMNI_READ_OUTDATED;
	}

	sample = sample_get_from_time(cache, time, false);

	/* TODO: Interpolation. */
	if (!IS_VALID(sample)) {
		return OMNI_READ_INVALID;
	}

	for (uint i = 0; i < cache->num_blocks; i++) {
		OmniBlockInfo *b_info = &cache->block_index[i];
		OmniBlock *block = &sample->blocks[i];
		OmniData omni_data;

		if (!(block->sflags & OMNI_BLOCK_STATUS_VALID)) {
			return OMNI_READ_INVALID;
		}

		omni_data.dtype = b_info->dtype;
		omni_data.dsize = b_info->dsize;
		omni_data.dcount = block->dcount;
		omni_data.data = block->data;

		if (!b_info->read(&omni_data, data)) {
			return OMNI_READ_INVALID;
		}

		if (!(block->sflags & OMNI_BLOCK_STATUS_CURRENT)) {
			result = OMNI_READ_OUTDATED;
		}
	}

	return result;
}

void OMNI_set_range(OmniCache *cache, float_or_uint time_initial, float_or_uint time_final, float_or_uint time_step)
{
	bool changed = false;

	assert(FU_FL_GT(time_step, 0.0f));
	assert(cache->ttype == time_initial.isf);
	assert(cache->ttype == time_final.isf);
	assert(cache->ttype == time_step.isf);
	assert(FU_LE(time_initial, time_final));

	if (!FU_EQ(time_initial, cache->tinitial)) {
		changed = true;

		cache->tinitial = time_initial;
	}

	if (!FU_EQ(time_final, cache->tfinal)) {
		changed = true;

		cache->tfinal = time_final;
	}

	if (!FU_EQ(time_step, cache->tstep)) {
		changed = true;

		cache->tstep = time_step;
	}

	/* TODO: Optionally clip/extend cache instead of freeing. */
	if (changed) {
		samples_free(cache);
	}
}

void OMNI_get_range(OmniCache *cache, float_or_uint *time_initial, float_or_uint *time_final, float_or_uint *time_step)
{
	if (time_initial) {
		*time_initial = cache->tinitial;
	}

	if (time_final) {
		*time_final = cache->tfinal;
	}

	if (time_step) {
		*time_initial = cache->tstep;
	}
}

bool OMNI_sample_is_valid(OmniCache *cache, float_or_uint time)
{
	/* TODO: Status flags should be generalized so that IS_VALID and IS_CURRENT can be used here. */
	if (!(cache->sflags & OMNICACHE_STATUS_VALID)) {
		return false;
	}

	return IS_VALID(sample_get_from_time(cache, time, false));
}

bool OMNI_sample_is_current(OmniCache *cache, float_or_uint time)
{
	/* TODO: Status flags should be generalized so that IS_VALID and IS_CURRENT can be used here. */
	if (!(cache->sflags & OMNICACHE_STATUS_CURRENT)) {
		return false;
	}

	return IS_CURRENT(sample_get_from_time(cache, time, false));
}

/* TODO: Consolidation should set the num_samples_array as to ignore trailing skipped samples (without children).
 * (same applies to sample_clear_from and such) */
void OMNI_consolidate(OmniCache *cache, OmniConsolidationFlags flags)
{
	/* TODO: Status flags should be generalized so that IS_VALID and IS_CURRENT can be used here. */
	if ((!(cache->sflags & OMNICACHE_STATUS_VALID) && (flags & (OMNI_CONSOL_FREE_INVALID | OMNI_CONSOL_FREE_OUTDATED))) ||
	    (!(cache->sflags & OMNICACHE_STATUS_CURRENT) && (flags & OMNI_CONSOL_FREE_OUTDATED)))
	{
		samples_free(cache);

		cache_set_flags(cache, OMNICACHE_STATUS_CURRENT);

		return;
	}

	/* Frees outdated and invalid samples. */
	if (flags & OMNI_CONSOL_FREE_OUTDATED) {
		samples_iterate(cache->samples, sample_remove_outdated, NULL, NULL);
	}
	/* Frees invalid samples. */
	else if (flags & OMNI_CONSOL_FREE_INVALID) {
		samples_iterate(cache->samples, sample_remove_invalid, NULL, NULL);
	}

	if (flags & OMNI_CONSOL_CONSOLIDATE) {
		if (!(cache->sflags & OMNICACHE_STATUS_VALID)) {
			samples_iterate(cache->samples, sample_mark_invalid, NULL, NULL);
		}
		else if (!(cache->sflags & OMNICACHE_STATUS_CURRENT)) {
			samples_iterate(cache->samples, sample_mark_outdated, NULL, NULL);
		}

		cache_set_flags(cache, OMNICACHE_STATUS_CURRENT);
	}
}

void OMNI_mark_outdated(OmniCache *cache)
{
	cache_unset_flags(cache, OMNICACHE_STATUS_CURRENT);
}

void OMNI_mark_invalid(OmniCache *cache)
{
	cache_unset_flags(cache, OMNICACHE_STATUS_VALID);
}

void OMNI_sample_mark_outdated(OmniCache *cache, float_or_uint time)
{
	OmniSample *sample = sample_get_from_time(cache, time, false);

	if (sample) {
		sample_mark_outdated(sample);
	}
}

void OMNI_sample_mark_invalid(OmniCache *cache, float_or_uint time)
{
	OmniSample *sample = sample_get_from_time(cache, time, false);

	if (sample) {
		sample_mark_invalid(sample);
	}
}

void OMNI_sample_clear(OmniCache *cache, float_or_uint time)
{
	OmniSample *sample = sample_get_from_time(cache, time, false);

	if (sample) {
		if (IS_ROOT(sample)) {
			sample_remove_root(sample);
		}
		else {
			OmniSample *prev = sample_prev(sample);
			prev->next = sample->next;

			sample_remove_list(sample);
		}
	}
}

/* TODO: Should mark samples from time even if sample at exact time does not exist. */
void OMNI_sample_mark_outdated_from(OmniCache *cache, float_or_uint time)
{
	OmniSample *sample = sample_get_from_time(cache, time, false);

	if (sample) {
		samples_iterate(sample, sample_mark_outdated, sample_mark_outdated, NULL);
	}
}

void OMNI_sample_mark_invalid_from(OmniCache *cache, float_or_uint time)
{
	OmniSample *sample = sample_get_from_time(cache, time, false);

	if (sample) {
		samples_iterate(sample, sample_mark_invalid, sample_mark_invalid, NULL);
	}
}

void OMNI_sample_clear_from(OmniCache *cache, float_or_uint time)
{
	OmniSample *sample = sample_get_from_time(cache, time, false);

	if (sample) {
		samples_iterate(sample,
		                          sample_remove_list,
		                          sample_remove_root,
		                          sample_clear_ref);
	}
}
