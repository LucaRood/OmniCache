/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __OMNI_OMNI_SERIAL_H__
#define __OMNI_OMNI_SERIAL_H__

#include "omni_types.h"

OmniSerial *serialize(const OmniCache *cache, bool serialize_data, uint *size);
OmniCache *deserialize(OmniSerial *serial, const OmniCacheTemplate *cache_temp);

#endif /* __OMNI_OMNI_SERIAL_H__ */
