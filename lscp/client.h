// client.h
//
/****************************************************************************
   liblscp - LinuxSampler Control Protocol API
   Copyright (C) 2004, rncbc aka Rui Nuno Capela. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

#ifndef __LSCP_CLIENT_H
#define __LSCP_CLIENT_H

#include "lscp/socket.h"

// Case unsensitive comparison substitutes.
#if defined(WIN32)
#define strcasecmp      stricmp
#define strncasecmp     strnicmp
#endif

#if defined(__cplusplus)
extern "C" {
#endif

//-------------------------------------------------------------------------
// Client data structures.

/** Buffer fill stream usage types. */
typedef enum _lscp_usage_t
{
    LSCP_USAGE_BYTES = 0,
    LSCP_USAGE_PERCENTAGE

} lscp_usage_t;


/** Engine info cache struct. */
typedef struct _lscp_engine_info_t
{
    char *        description;
    char *        version;

} lscp_engine_info_t;


/** Channel info cache struct. */
typedef struct _lscp_channel_info_t
{
    char *        engine_name;
    char *        audio_type;
    int           audio_channels;
    char **       audio_routing;
    char *        instrument_file;
    int           instrument_nr;
    char *        midi_type;
    int           midi_port;
    int           midi_channel;
    float         volume;

} lscp_channel_info_t;


/** Buffer fill cache struct. */
typedef struct _lscp_buffer_fill_t
{
    unsigned int  stream_id;
    unsigned long stream_usage;

} lscp_buffer_fill_t;


//-------------------------------------------------------------------------
// Client socket main structure.

struct _lscp_client_t;

/** Client callback procedure prototype. */
typedef lscp_status_t (*lscp_client_proc_t)
(
    struct _lscp_client_t *pClient,
    const char *pchBuffer,
    int cchBuffer,
    void *pvData
);

/** Client descriptor struct. */
typedef struct _lscp_client_t {

    // Client socket stuff.
    lscp_client_proc_t  pfnCallback;
    void *              pvData;
    lscp_socket_agent_t tcp;
    lscp_socket_agent_t udp;
    // Session identifier.
    char *              sessid;
    // Info struct caches.
    char **             audio_types;
    char **             midi_types;
    char **             engines;
    lscp_engine_info_t  engine_info;
    lscp_channel_info_t channel_info;
    // Result and error status.
    char *              pszResult;
    int                 iErrno;
    // Stream buffers status.
    lscp_buffer_fill_t *buffer_fill;
    int                 iStreamCount;

} lscp_client_t;


//-------------------------------------------------------------------------
// Client versioning teller fuunction.

const char *            lscp_client_package             (void);
const char *            lscp_client_version             (void);
const char *            lscp_client_build               (void);


//-------------------------------------------------------------------------
// Client socket functions.

lscp_client_t *         lscp_client_create              (char *pszHost, int iPort, lscp_client_proc_t pfnCallback, void *pvData);
lscp_status_t           lscp_client_join                (lscp_client_t *pClient);
lscp_status_t           lscp_client_destroy             (lscp_client_t *pClient);

lscp_status_t           lscp_client_call                (lscp_client_t *pClient, const char *pchBuffer, int cchBuffer, char *pchResult, int *pcchResult);

lscp_status_t           lscp_client_subscribe           (lscp_client_t *pClient);
lscp_status_t           lscp_client_unsubscribe         (lscp_client_t *pClient);


//-------------------------------------------------------------------------
// Client common protocol functions.

lscp_status_t           lscp_client_query               (lscp_client_t *pClient, const char *pszQuery);
const char *            lscp_client_get_result          (lscp_client_t *pClient );
int                     lscp_client_get_errno           (lscp_client_t *pClient );


//-------------------------------------------------------------------------
// Client command protocol functions.

const char **           lscp_get_available_audio_types  (lscp_client_t *pClient);
const char **           lscp_get_available_midi_types   (lscp_client_t *pClient);

lscp_status_t           lscp_load_instrument            (lscp_client_t *pClient, const char *pszFileName, int iInstrIndex, int iSamplerChannel);
lscp_status_t           lscp_load_engine                (lscp_client_t *pClient, const char *pszEngineName, int iSamplerChannel);
int                     lscp_get_channels               (lscp_client_t *pClient);
lscp_status_t           lscp_add_channel                (lscp_client_t *pClient);
lscp_status_t           lscp_remove_channel             (lscp_client_t *pClient, int iSamplerChannel);
const char **           lscp_get_available_engines      (lscp_client_t *pClient);

lscp_engine_info_t *    lscp_get_engine_info            (lscp_client_t *pClient, const char *pszEngineName);
lscp_channel_info_t *   lscp_get_channel_info           (lscp_client_t *pClient, int iSamplerChannel);

int                     lscp_get_channel_voice_count    (lscp_client_t *pClient, int iSamplerChannel);
int                     lscp_get_channel_stream_count   (lscp_client_t *pClient, int iSamplerChannel);

lscp_buffer_fill_t *    lscp_get_channel_buffer_fill    (lscp_client_t *pClient, lscp_usage_t iUsageType, int iSamplerChannel);

lscp_status_t           lscp_set_channel_audio_type     (lscp_client_t *pClient, int iSamplerChannel, const char *pszAudioType);
lscp_status_t           lscp_set_channel_audio_channel  (lscp_client_t *pClient, int iSamplerChannel, int iAudioOut, int iAudioIn);

lscp_status_t           lscp_set_channel_midi_type      (lscp_client_t *pClient, int iSamplerChannel, const char *pszMidiType);
lscp_status_t           lscp_set_channel_midi_port      (lscp_client_t *pClient, int iSamplerChannel, int iMidiPort);
lscp_status_t           lscp_set_channel_midi_channel   (lscp_client_t *pClient, int iSamplerChannel, int iMidiChannel);
lscp_status_t           lscp_set_channel_volume         (lscp_client_t *pClient, int iSamplerChannel, float fVolume);

lscp_status_t           lscp_reset_channel              (lscp_client_t *pClient, int iSamplerChannel);


#if defined(__cplusplus)
}
#endif

#endif // __LSCP_CLIENT_H

// end of client.h
