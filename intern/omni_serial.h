/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __OMNI_OMNI_SERIAL_H__
#define __OMNI_OMNI_SERIAL_H__

#include "omni_types.h"

typedef struct OmniSerialCache {
	char id[MAX_NAME];

	OmniTimeType ttype;
	float_or_uint tinitial;
	float_or_uint tfinal;
	float_or_uint tstep;

	OmniCacheFlags flags;

	uint num_blocks;
	uint num_samples_array;
	uint num_samples_tot;

	uint msize;
} OmniSerialCache;

typedef struct OmniSerialBlockInfo {
	char id[MAX_NAME];

	OmniDataType dtype;
	uint dsize;

	OmniBlockFlags flags;
} OmniSerialBlockInfo;

uint serialize(OmniSerial **serial, const OmniCache *cache, bool serialize_data);
OmniCache *deserialize(OmniSerial *serial, OmniCacheTemplate *cache_temp);

#endif /* __OMNI_OMNI_SERIAL_H__ */
