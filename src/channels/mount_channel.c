/*
 * channels constructor / destructor
 *
 * Copyright (c) 2012, LiteStack, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <glib.h>
#include "src/main/etag.h"
#include "src/loader/sel_ldr.h"
#include "src/main/manifest_setup.h"
#include "src/main/manifest_parser.h"
#include "src/channels/preload.h"
#include "src/channels/prefetch.h"
#include "src/channels/mount_channel.h"

static GData *aliases;

char *StringizeChannelSourceType(enum ChannelSourceType type)
{
  char *prefix[] = CHANNEL_SOURCE_PREFIXES;

  assert(type >= ChannelRegular && type < ChannelSourceTypeNumber);
  return prefix[type];
}

/* return source type inferred from the source file (or url) */
static enum ChannelSourceType GetSourceType(const char *name)
{
  enum ChannelSourceType type;

  assert(name != NULL);

  /* unlike local network channels always contain ':'s */
  if(strchr(name, ':') == NULL)
    type = GetChannelSource(name);
  else
    type = GetChannelProtocol(name);

  ZLOGFAIL(type == ChannelSourceTypeNumber,
      EPROTONOSUPPORT, "cannot detect source of %s", name);
  return type;
}

/* return the channel index by channel alias */
static int SelectNextChannel(const char *alias)
{
  static int current_channel = RESERVED_CHANNELS;

  assert(alias != NULL);

  if(STREQ(alias, STDIN)) return STDIN_FILENO;
  if(STREQ(alias, STDOUT)) return STDOUT_FILENO;
  if(STREQ(alias, STDERR)) return STDERR_FILENO;
  return current_channel++;
}

/* construct and initialize the channel */
static void ChannelCtor(struct NaClApp *nap, char **tokens)
{
  struct ChannelDesc *channel;
  int code = -1;
  int index;
  int i;

  assert(nap != NULL);
  assert(tokens != NULL);
  assert(nap->system_manifest != NULL);
  assert(nap->system_manifest->channels != NULL);

  /* check alias for duplicates and update the list */
  ZLOGFAIL(g_datalist_get_data(&aliases, tokens[ChannelAlias]) != NULL,
      EFAULT, "%s is already allocated", tokens[ChannelAlias]);
  g_datalist_set_data(&aliases, tokens[ChannelAlias], "");

  /*
   * pick the channel and check if the channel is available,
   * then allocate space to store the channel information
   */
  index = SelectNextChannel(tokens[ChannelAlias]);
  ZLOGFAIL(index >= nap->system_manifest->channels_count,
      EFAULT, "uninitialized standard channels detected");
  channel = &nap->system_manifest->channels[index];

  /* set common general fields */
  channel->type = ATOI(tokens[ChannelAccessType]);
  ZLOGFAIL(channel->type < SGetSPut || channel->type > RGetRPut,
      EFAULT, "invalid access type for %s", tokens[ChannelAlias]);
  channel->name = tokens[ChannelName];
  channel->alias = tokens[ChannelAlias];
  channel->source = GetSourceType((char*)channel->name);

  /* initialize the channel tag */
  if(CHANNELS_ETAG_ENABLED)
  {
    channel->tag = TagCtor();
    memset(channel->digest, 0, TAG_DIGEST_SIZE);
    memset(channel->control, 0, TAG_DIGEST_SIZE);
  }

  /* limits and counters. initialize all field explicitly */
  channel->eof = 0;
  for(i = 0; i < IOLimitsCount; ++i)
  {
    channel->counters[i] = 0;
    channel->limits[i] = ATOI(tokens[i + ChannelGets]);
    ZLOGFAIL(channel->limits[i] < 0, EFAULT,
        "%s has invalid %dth limit", tokens[ChannelAlias], i);
  }

  /* mount given channel */
  switch(channel->source)
  {
    case ChannelRegular:
    case ChannelCharacter:
    case ChannelFIFO:
      code = PreloadChannelCtor(channel);
      break;
    case ChannelTCP:
      code = PrefetchChannelCtor(channel);
      break;
    default:
      ZLOGFAIL(1, EPROTONOSUPPORT, "%s has invalid type %s",
          channel->alias, StringizeChannelSourceType(channel->source));
      break;
  }
  ZLOGFAIL(code, EFAULT, "cannot allocate %s", channel->alias);
  channel->mounted = MOUNTED;
}

/* close channel and deallocate its resources */
static void ChannelDtor(struct ChannelDesc *channel)
{
  assert(channel != NULL);

  /* quit if channel isn't mounted */
  if(channel->mounted != MOUNTED) return;

  switch(channel->source)
  {
    case ChannelRegular:
    case ChannelCharacter:
    case ChannelFIFO:
      PreloadChannelDtor(channel);
      break;
    case ChannelTCP:
      /*
       * since there is a chance to hang up upon the network channels
       * finalization in case if session crashed, the channel destructor
       * just skips it
       */
      if(GetExitCode() == 0)
        PrefetchChannelDtor(channel);
      break;
    default:
      ZLOG(LOG_ERR, "%s has invalid type %s",
          channel->alias, StringizeChannelSourceType(channel->source));
      break;
  }
  channel->mounted = !MOUNTED;
}

void ChannelsCtor(struct NaClApp *nap)
{
  int i;
  char *values[MAX_CHANNELS_NUMBER];
  struct SystemManifest *mft;

  assert(nap != NULL);
  assert(nap->system_manifest != NULL);

  /* allocate list to detect duplicate channels aliases */
  assert(aliases == NULL);
  g_datalist_init(&aliases);

  /*
   * calculate channels count. maximum allowed (MAX_CHANNELS_NUMBER - 1)
   * channels, minimum - RESERVED_CHANNELS
   */
  mft = nap->system_manifest;
  mft->channels_count =
      GetValuesByKey(MFT_CHANNEL, values, MAX_CHANNELS_NUMBER);
  ZLOGFAIL(mft->channels_count >= MAX_CHANNELS_NUMBER,
      ENFILE, "channels number reached maximum");
  ZLOGFAIL(mft->channels_count < RESERVED_CHANNELS,
      EFAULT, "not all standard channels are provided");

  /* allocate memory for channels pointers */
  mft->channels = g_malloc0(mft->channels_count * sizeof *mft->channels);

  /* parse and mount channels */
  for(i = 0; i < mft->channels_count; ++i)
  {
    char *tokens[CHANNEL_ATTRIBUTES + 1];
    int count = ParseValue(values[i], ",", tokens, CHANNEL_ATTRIBUTES + 1);

    /* fail if invalid number of attributes detected */
    ZLOGFAIL(count != CHANNEL_ATTRIBUTES, EFAULT,
        "%s has %d attributes instead of %d", tokens[ChannelAlias],
        count, CHANNEL_ATTRIBUTES);

    /* construct and initialize channel */
    ChannelCtor(nap, tokens);
  }
  g_datalist_clear(&aliases);

  /* 2nd pass for the network channels if name service specified */
  KickPrefetchChannels(nap);
}

void ChannelsFinalizer(struct NaClApp *nap)
{
  int i;

  /* exit if channels are not constructed */
  assert(nap != NULL);
  if(nap->system_manifest == NULL) return;
  if(nap->system_manifest->channels == NULL) return;

  for(i = 0; i < nap->system_manifest->channels_count; ++i)
  {
    ChannelDtor(&nap->system_manifest->channels[i]);
    if(CHANNELS_ETAG_ENABLED)
      TagUpdate(nap->user_tag,
          (const char*)nap->system_manifest->channels[i].digest, TAG_DIGEST_SIZE);
  }
}

void ChannelsDtor(struct NaClApp *nap)
{
  assert(nap != NULL);
  if(nap->system_manifest != NULL)
  {
    g_free(nap->system_manifest->channels);
    nap->system_manifest->channels = NULL;
    if(aliases != NULL) g_datalist_clear(&aliases);
  }
}
