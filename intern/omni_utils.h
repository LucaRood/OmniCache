/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __OMNI_OMNI_UTILS_H__
#define __OMNI_OMNI_UTILS_H__

#include "omni_types.h"
#include "utils.h"

#define DATA_SIZE(dtype, dsize) (dtype == OMNI_DATA_GENERIC) ? dsize : OMNI_DATA_TYPE_SIZE[dtype];

#define IS_VALID(sample) (sample && (sample->sflags & OMNI_SAMPLE_STATUS_VALID) && !(sample->sflags & OMNI_SAMPLE_STATUS_SKIP))
#define IS_CURRENT(sample) (sample && (sample->sflags & OMNI_SAMPLE_STATUS_VALID) && (sample->sflags & OMNI_SAMPLE_STATUS_VALID) && !(sample->sflags & OMNI_SAMPLE_STATUS_SKIP))
#define IS_ROOT(sample) FU_FL_EQ(sample->toffset, 0.0f)

typedef void (*iter_callback)(OmniSample *sample);

void block_set_flags(OmniBlock *block, OmniBlockStatusFlags flags);
void block_unset_flags(OmniBlock *block, OmniBlockStatusFlags flags);

void meta_set_flags(OmniSample *sample, OmniBlockStatusFlags flags);
void meta_unset_flags(OmniSample *sample, OmniBlockStatusFlags flags);

void sample_set_flags(OmniSample *sample, OmniSampleStatusFlags flags);
void sample_unset_flags(OmniSample *sample, OmniSampleStatusFlags flags);

sample_time gen_sample_time(OmniCache *cache, float_or_uint time);

void samples_iterate(OmniSample *start, iter_callback list, iter_callback root, iter_callback first);
OmniSample *sample_prev(OmniSample *sample);

void resize_sample_array(OmniCache *cache, uint size);
void init_sample_blocks(OmniSample *sample);

void update_block_parents(OmniCache *cache);

#endif /* __OMNI_OMNI_UTILS_H__ */
