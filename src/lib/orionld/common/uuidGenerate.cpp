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
#include <uuid/uuid.h>                                         // uuid_t, uuid_generate_time_safe, uuid_unparse_lower
#include <string.h>                                            // strlen, strncpy, memcpy
#include <openssl/evp.h>                                       // EVP_Digest, EVP_sha1 (libcrypto - already linked)

extern "C"
{
#include "ktrace/kTrace.h"                                     // KT_*
}

#include "orionld/common/uuidGenerate.h"                       // Own interface



// -----------------------------------------------------------------------------
//
// uuidGenerate -
//
char* uuidGenerate(char* buf, int bufSize, const char* prefix)
{
  uuid_t uuid;
  int    bufIx      = 0;
  int    minBufSize = 37;

  if (prefix != NULL)
    minBufSize += strlen(prefix);

  if (bufSize < minBufSize)
    KT_X(1, "Implementation Error (not enough room to generate a UUID (%d bytes needed, %d supplied)", minBufSize, bufSize);

  uuid_generate_time_safe(uuid);

  if (prefix != NULL)
  {
    strncpy(buf, prefix, bufSize);
    bufIx = strlen(prefix);
  }

  uuid_unparse_lower(uuid, &buf[bufIx]);

  return &buf[bufIx];
}



// -----------------------------------------------------------------------------
//
// UUIDV5_NAMESPACE_URL - the RFC 4122 namespace UUID for URLs (6ba7b811-9dad-11d1-80b4-00c04fd430c8)
//
// Our names are URN-like (entityId|attr|datasetId|observedAt), so the URL namespace is the
// natural choice. Fixed forever - changing it would change every derived instanceId.
//
static const unsigned char uuidV5NamespaceUrl[16] =
{
  0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
};



// -----------------------------------------------------------------------------
//
// uuidV5Generate - deterministic, name-based UUID (RFC 4122 version 5, SHA-1)
//
char* uuidV5Generate(char* buf, int bufSize, const char* prefix, const char* name, int nameLen)
{
  int bufIx      = 0;
  int minBufSize = 37;

  if (prefix != NULL)
    minBufSize += strlen(prefix);

  if (bufSize < minBufSize)
    KT_X(1, "Implementation Error (not enough room to generate a UUID (%d bytes needed, %d supplied)", minBufSize, bufSize);

  if (nameLen < 0)
    nameLen = (name != NULL)? strlen(name) : 0;

  //
  // RFC 4122 v5: uuid = SHA1(namespace || name), then overwrite version (5) and variant (RFC 4122) bits.
  // EVP_Digest is the OpenSSL-3-safe one-shot digest API (SHA1() itself is deprecated in 3.0).
  //
  unsigned char  hash[EVP_MAX_MD_SIZE];
  unsigned int   hashLen = 0;
  EVP_MD_CTX*    ctx     = EVP_MD_CTX_new();

  if ((ctx == NULL) ||
      (EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) != 1) ||
      (EVP_DigestUpdate(ctx, uuidV5NamespaceUrl, sizeof(uuidV5NamespaceUrl)) != 1) ||
      (EVP_DigestUpdate(ctx, name, nameLen) != 1) ||
      (EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) ||
      (hashLen < 16))
  {
    if (ctx != NULL)
      EVP_MD_CTX_free(ctx);
    KT_X(1, "Implementation Error (SHA-1 digest for UUIDv5 failed)");
  }
  EVP_MD_CTX_free(ctx);

  uuid_t uuid;
  memcpy(uuid, hash, 16);
  uuid[6] = (uuid[6] & 0x0F) | 0x50;  // version 5
  uuid[8] = (uuid[8] & 0x3F) | 0x80;  // RFC 4122 variant

  if (prefix != NULL)
  {
    strncpy(buf, prefix, bufSize);
    bufIx = strlen(prefix);
  }

  uuid_unparse_lower(uuid, &buf[bufIx]);

  return &buf[bufIx];
}
