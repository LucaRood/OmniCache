/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __OMNI_OMNICACHE_H__
#define __OMNI_OMNICACHE_H__

#include <assert.h>

#include "types.h"

#define MAX_NAME 64

/*********
 * Enums *
 *********/

typedef enum OmniReadResult {
	OMNI_READ_INVALID	= 0,
	OMNI_READ_OUTDATED	= 1,
	OMNI_READ_INTERP	= 2,
	OMNI_READ_EXACT		= 3,
} OmniReadResult;

typedef enum OmniTimeType {
	OMNI_TIME_INT		= 0, /* Discrete integer time. */
	OMNI_TIME_FLOAT		= 1, /* Continuous floating point time. */
} OmniTimeType;

typedef enum OmniDataType {
	OMNI_DATA_GENERIC	= 0, /* Black box data not manipulated by OmniCache. */
	OMNI_DATA_META		= 1,
	OMNI_DATA_FLOAT		= 2,
	OMNI_DATA_FLOAT3	= 3,
	OMNI_DATA_INT		= 4,
	OMNI_DATA_INT3		= 5,
	OMNI_DATA_MAT3		= 6,
	OMNI_DATA_MAT4		= 7,
	OMNI_DATA_REF		= 8, /* Reference to a constant OnmiCache library block. */
	OMNI_DATA_TREF		= 9, /* Transformed reference to a constant OmniCache library block (includes MAT4). */

	OMNI_NUM_DTYPES	= 10, /* Number of data types (should always be the last entry). */
} OmniDataType;

/*********
 * Types *
 *********/

typedef struct OmniCache OmniCache;

/* Transformed reference. */
typedef struct OmniTRef {
	uint index;
	float mat[4][4];
} OmniTRef;

/*************
 * Callbacks *
 *************/

typedef struct OmniData {
	OmniDataType dtype;
	uint dsize;
	uint dcount;
	void *data;
} OmniData;

typedef struct OmniInterpData {
	OmniData *target;
	OmniData *prev;
	OmniData *next;
	float_or_uint ttarget;
	float_or_uint tprev;
	float_or_uint tnext;
} OmniInterpData;

typedef uint (*OmniCountCallback)(void *user_data);
typedef bool (*OmniReadCallback)(OmniData *omnic_data, void *user_data);
typedef bool (*OmniWriteCallback)(OmniData *omnic_data, void *user_data);
typedef bool (*OmniInterpCallback)(OmniInterpData *interp_data);

typedef bool (*OmnicMetaGenCallback)(void *user_data, void *result);

/*********
 * Flags *
 *********/

typedef enum OmniBlockFlags {
	OMNI_BLOCK_FLAG_CONTINUOUS	= (1 << 0), /* Continuous data that can be interpolated. */
} OmniBlockFlags;

typedef enum OmniCacheFlags {
	OMNICACHE_FLAG_FRAMED		= (1 << 0), /* Time in frames instead of seconds. */
	OMNICACHE_FLAG_INTERPOLATE	= (1 << 1), /* Interpolation is enabled. */
} OmniCacheFlags;


/*************
 * Templates *
 *************/

typedef struct OmniCacheTemplate {
	OmniTimeType time_type;

	/* Initial time and default step size.
	 * - `float` if `ttype` is `OMNI_SAMPLING_FLOAT`;
	 * - `uint` if `ttype` is `OMNI_SAMPLING_INT`.*/
	float_or_uint time_initial;
	float_or_uint time_final;
	float_or_uint time_step;

	OmniCacheFlags flags;

	uint meta_size;

	uint num_blocks;

	OmnicMetaGenCallback meta_gen;
} OmniCacheTemplate;

typedef struct OmniBlockTemplate {
	char name[MAX_NAME];

	OmniDataType data_type;
	uint data_size; /* Only required if `dtype` == `OMNI_DATA_GENERIC` */

	OmniBlockFlags flags;

	OmniCountCallback count;
	OmniReadCallback read;
	OmniWriteCallback write;
	OmniInterpCallback interp;
} OmniBlockTemplate;

typedef OmniBlockTemplate OmniBlockTemplateArray[];

/*****************
 * API Functions *
 *****************/

float_or_uint OMNI_f_to_fu(float val);
float_or_uint OMNI_u_to_fu(uint val);

OmniCache *OMNI_new(OmniCacheTemplate *c_temp, OmniBlockTemplateArray b_temp);
void OMNI_free(OmniCache *cache);

bool OMNI_sample_write(OmniCache *cache, float_or_uint time, void *data);
OmniReadResult OMNI_sample_read(OmniCache *cache, float_or_uint time, void *data);

void OMNI_set_range(OmniCache *cache, float_or_uint time_initial, float_or_uint time_final, float_or_uint time_step);

bool OMNI_sample_is_valid(OmniCache *cache, float_or_uint time);
bool OMNI_sample_is_current(OmniCache *cache, float_or_uint time);

void OMNI_sample_mark_old(OmniCache *cache, float_or_uint time);
void OMNI_sample_mark_invalid(OmniCache *cache, float_or_uint time);
void OMNI_sample_clear(OmniCache *cache, float_or_uint time);

void OMNI_sample_mark_old_from(OmniCache *cache, float_or_uint time);
void OMNI_sample_mark_invalid_from(OmniCache *cache, float_or_uint time);
void OMNI_sample_clear_from(OmniCache *cache, float_or_uint time);

#endif /* __OMNI_OMNICACHE_H__ */