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
	uint size = sizeof(OmniCacheDef) + (sizeof(OmniBlockInfoDef) * cache->def.num_blocks);

	s = malloc(size);
	*serial = s;

	/* cache */
	{
		OmniCacheDef *temp = (OmniCacheDef *)s;

		memcpy(temp, cache, sizeof(OmniCacheDef));

		/* TODO: Respect serialize_data. */
		temp->num_samples_array = 0;
		temp->num_samples_tot = 0;

		INCREMENT_SERIAL();
	}

	/* block_index */
	{
		OmniBlockInfoDef *temp = (OmniBlockInfoDef *)s;

		for (uint i = 0; i < cache->def.num_blocks; i++) {
			OmniBlockInfo *block = &cache->block_index[i];

			memcpy(temp, block, sizeof(OmniBlockInfoDef));

			INCREMENT_SERIAL();
		}
	}

	return size;
}

OmniCache *deserialize(OmniSerial *serial, const OmniCacheTemplate *cache_temp)
{
	OmniSerial *s = (OmniSerial *)serial;
	OmniCache *cache;

	/* cache */
	{
		OmniCacheDef *temp = (OmniCacheDef *)s;

		if (cache_temp &&
		    strncmp(temp->id, cache_temp->id, MAX_NAME) != 0)
		{
			fprintf(stderr, "OmniCache: Deserialization falied, cache type mismatch.\n");

			return NULL;
		}

		cache = malloc(sizeof(OmniCache));

		memcpy(cache, temp, sizeof(OmniCacheDef));

		cache_set_status(cache, OMNI_STATUS_CURRENT);

		/* TODO: Data deserialization. */
		cache->num_samples_alloc = 0;

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
		OmniBlockInfoDef *temp = (OmniBlockInfoDef *)s;

		cache->block_index = malloc(sizeof(OmniBlockInfo) * cache->def.num_blocks);

		for (uint i = 0; i < cache->def.num_blocks; i++) {
			OmniBlockInfo *b_info = &cache->block_index[i];
			const OmniBlockTemplate *b_temp = NULL;

			if (cache_temp) {
				b_temp = block_template_find(cache_temp, temp->id, i);
			}

			memcpy(b_info, temp, sizeof(OmniBlockInfoDef));

			b_info->parent = cache;

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
