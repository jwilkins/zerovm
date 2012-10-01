/*
 * etag engine api
 *
 *  Created on: Sep 15, 2012
 *      Author: d'b
 */

#ifndef ETAG_H_
#define ETAG_H_

#include <stdint.h>
#include <openssl/sha.h>

#define TAG_ENGINE_DISABLED "disabled"
#define TAG_CONTEXT_SIZE sizeof(SHA_CTX) /* tag size */
#define TAG_BINARY_SIZE SHA_DIGEST_LENGTH /* tag size */
#define TAG_DIGEST_SIZE SHA_DIGEST_LENGTH * 2 + 1 /* tag digest size */

/* etag engine construction */
void TagEngineCtor();

/* return the etag_enabled state */
int TagEngineEnabled();

/*
 * initialize the hash context in provided space
 * return 0 if everything is ok
 */
int TagCtor(void *ctx);

/*
 * calculates digest from the context. can be used consequently
 * note: "digest" must have enough space to hold the digest
 */
void TagDigest(void *ctx, char *digest);

/*
 * update etag with the given buffer.
 * returns 0 if all ok or -1 if failed
 */
int TagUpdate(void *ctx, const char *buffer, int32_t size);

#endif /* ETAG_H_ */