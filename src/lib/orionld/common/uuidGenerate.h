#ifndef SRC_LIB_ORIONLD_COMMON_UUIDGENERATE_H_
#define SRC_LIB_ORIONLD_COMMON_UUIDGENERATE_H_

/*
*
* Copyright 2019 FIWARE Foundation e.V.
*
* This file is part of Orion-LD Context Broker.
*
* Orion-LD Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion-LD Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion-LD Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* orionld at fiware dot org
*
* Author: Ken Zangelin
*/



// ----------------------------------------------------------------------------
//
// uuidGenerate -
//
extern char* uuidGenerate(char* buf, int bufSize, const char* prefix);



// ----------------------------------------------------------------------------
//
// uuidV5Generate - deterministic, name-based UUID (RFC 4122 version 5, SHA-1)
//
// Unlike uuidGenerate (random/time-based), this produces the SAME UUID every time for the
// same 'name'. Used to derive a reproducible TRoE attribute instanceId from the business key
// (entityId|attribute|datasetId|observedAt), so that re-sending the same observation yields the
// same instanceId - the basis for idempotent temporal ingestion.
//
extern char* uuidV5Generate(char* buf, int bufSize, const char* prefix, const char* name, int nameLen);

#endif  // SRC_LIB_ORIONLD_COMMON_UUIDGENERATE_H_
