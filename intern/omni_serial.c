/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "omni_serial.h"

#include "omnicache.h"
#include "omni_utils.h"


#define INCREMENT_SERIAL() s = (OmniSerial *)(++temp)

/* TODO: Data serialization. */
uint serialize(OmniSerial **serial, const OmniCache *cache, bool UNUSED(serialize_data))
{
	OmniSerial *s;
	uint size = sizeof(OmniSerialCache) + (sizeof(OmniSerialBlockInfo) * cache->num_blocks);

	s = malloc(size);
	*serial = s;

	/* cache */
	{
		OmniSerialCache *temp = (OmniSerialCache *)s;

		strncpy(temp->id, cache->id, MAX_NAME);

		temp->ttype = cache->ttype;
		temp->tinitial = cache->tinitial;
		temp->tfinal = cache->tfinal;
		temp->tstep = cache->tstep;

		temp->flags = cache->flags;

		temp->num_blocks = cache->num_blocks;

		/* TODO: Respect serialize_data. */
		temp->num_samples_array = 0;
		temp->num_samples_tot = 0;

		temp->msize = cache->msize;

		INCREMENT_SERIAL();
	}

	/* block_index */
	{
		OmniSerialBlockInfo *temp = (OmniSerialBlockInfo *)s;

		for (uint i = 0; i < cache->num_blocks; i++) {
			OmniBlockInfo *block = &cache->block_index[i];

			strncpy(temp->id, block->id, MAX_NAME);

			temp->dtype = block->dtype;
			temp->dsize = block->dsize;
			temp->flags = block->flags;

			INCREMENT_SERIAL();
		}
	}

	return size;
}

OmniCache *deserialize(OmniSerial *serial, OmniCacheTemplate *cache_temp)
{
	OmniSerial *s = (OmniSerial *)serial;
	OmniCache *cache;

	/* cache */
	{
		OmniSerialCache *temp = (OmniSerialCache *)s;

		if (cache_temp &&
		    strncmp(temp->id, cache_temp->id, MAX_NAME) != 0)
		{
			fprintf(stderr, "OmniCache: Deserialization falied, cache type mismatch.\n");

			return NULL;
		}

		cache = malloc(sizeof(OmniCache));

		strncpy(cache->id, temp->id, MAX_NAME);

		cache->ttype = temp->ttype;
		cache->tinitial = temp->tinitial;
		cache->tfinal = temp->tfinal;
		cache->tstep = temp->tstep;

		cache->flags = temp->flags;
		cache_set_status(cache, OMNI_STATUS_CURRENT);

		cache->num_blocks = temp->num_blocks;

		/* TODO: Data deserialization. */
		cache->num_samples_alloc = 0;
		cache->num_samples_array = 0;
		cache->num_samples_tot = 0;

		cache->msize = temp->msize;

		cache->samples = NULL;

		if (cache_temp) {
			cache->meta_gen = cache_temp->meta_gen;
		}
		else {
			cache->meta_gen = NULL;
		}

		INCREMENT_SERIAL();
	}

	/* block_index */
	{
		OmniSerialBlockInfo *temp = (OmniSerialBlockInfo *)s;

		cache->block_index = malloc(sizeof(OmniBlockInfo) * cache->num_blocks);

		for (uint i = 0; i < cache->num_blocks; i++) {
			OmniBlockInfo *b_info = &cache->block_index[i];
			OmniBlockTemplate *b_temp = NULL;

			if (cache_temp) {
				b_temp = block_template_find(cache_temp, temp->id, i);
			}

			b_info->parent = cache;

			strncpy(b_info->id, temp->id, MAX_NAME);

			b_info->dtype = temp->dtype;
			b_info->dsize = temp->dsize;

			b_info->flags = temp->flags;

			if (b_temp) {
				b_info->count = b_temp->count;
				b_info->read = b_temp->read;
				b_info->write = b_temp->write;
				b_info->interp = b_temp->interp;
			}

			INCREMENT_SERIAL();
		}
	}

	return cache;
}

#undef INCREMENT_SERIAL
