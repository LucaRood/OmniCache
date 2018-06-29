/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __OMNI_OMNI_TYPES_H__
#define __OMNI_OMNI_TYPES_H__

#include "types.h"
#include "omnicache.h"


/* Size of the data types in `omnicache.h` (keep in sync). */
static const uint OMNI_DATA_TYPE_SIZE[] = {
	0,						/* OMNI_DATA_GENERIC */
	0,						/* OMNI_DATA_META */
	sizeof(float),			/* OMNI_DATA_FLOAT */
	sizeof(float[3]),		/* OMNI_DATA_FLOAT3 */
	sizeof(int),			/* OMNI_DATA_INT */
	sizeof(int[3]),			/* OMNI_DATA_INT3 */
	sizeof(float[3][3]),	/* OMNI_DATA_MAT3 */
	sizeof(float[4][4]),	/* OMNI_DATA_MAT4 */
	sizeof(uint),			/* OMNI_DATA_REF */
	sizeof(OmniTRef),		/* OMNI_DATA_TREF */
};

static_assert((sizeof(OMNI_DATA_TYPE_SIZE) / sizeof(*OMNI_DATA_TYPE_SIZE)) == OMNI_NUM_DTYPES, "OmniCache: data type mismatch");

typedef struct sample_time {
	OmniTimeType ttype;
	uint index;
	float_or_uint offset;
} sample_time;

/* Block */

typedef struct OmniBlockInfo {
	struct OmniCache *parent;

	char name[MAX_NAME];

	OmniDataType dtype;
	uint dsize;

	OmniBlockFlags flags;

	OmniCountCallback count;
	OmniReadCallback read;
	OmniWriteCallback write;
	OmniInterpCallback interp;
} OmniBlockInfo;

typedef enum OmniBlockStatusFlags {
	OMNI_BLOCK_STATUS_INITED	= (1 << 0),
	OMNI_BLOCK_STATUS_VALID		= (1 << 1),
	OMNI_BLOCK_STATUS_CURRENT	= (1 << 2),
} OmniBlockStatusFlags;

typedef struct OmniBlock {
	struct OmniSample *parent;

	OmniBlockStatusFlags sflags;
	uint dcount;

	void *data;
} OmniBlock;

typedef struct OmniMetaBlock {
	OmniBlockStatusFlags sflags;

	void *data;
} OmniMetaBlock;


/* Sample */

typedef enum OmniSampleStatusFlags {
	OMNI_SAMPLE_STATUS_INITED	= (1 << 0), /* Sample has been initialized. (just means it was processed and accounted for, data can still be garbage) */
	OMNI_SAMPLE_STATUS_VALID	= (1 << 1), /* Data is valid (not garbage). */
	OMNI_SAMPLE_STATUS_CURRENT	= (1 << 2), /* Data is up to date. */
	OMNI_SAMPLE_STATUS_SKIP		= (1 << 3), /* Unused sample. */
} OmniSampleStatusFlags;

typedef struct OmniSample {
	struct OmniSample *next;
	struct OmniCache *parent;
	OmniMetaBlock meta;

	OmniSampleStatusFlags sflags;

	uint tindex;
	float_or_uint toffset;

	OmniBlock *blocks;
} OmniSample;


/* Cache */

typedef enum OmniCacheStatusFlags {
	OMNICACHE_STATUS_SYNCED		= (1 << 0), /* Set if all blocks are valid in all existing samples. */
	OMNICACHE_STATUS_COMPLETE	= (1 << 1), /* Set if the whole frame range is cached (valid). */
} OmniCacheStatusFlags;

typedef struct OmniCache {
	OmniTimeType ttype;
	float_or_uint tinitial;
	float_or_uint tfinal;
	float_or_uint tstep;

	OmniCacheFlags flags;
	OmniCacheStatusFlags sflags;

	uint num_blocks;
	uint num_samples_alloc; /* Number of samples allocated in the array. */
	uint num_samples_array; /* Number of samples initialized in the array. */
	uint num_samples_tot; /* Total number of samples initialized (including sub-samples) */

	uint msize;

	OmniBlockInfo *block_index;
	OmniSample *samples;

	OmnicMetaGenCallback meta_gen;
} OmniCache;

#endif /* __OMNI_OMNI_TYPES_H__ */
