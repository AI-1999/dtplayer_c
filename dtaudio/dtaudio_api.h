#ifndef DTAUDIO_API_H
#define DTAUDIO_API_H

#include "dt_state.h"
#include <stdint.h>

#include "dtaudio_para.h"

int dtaudio_init (void **audio_priv, dtaudio_para_t * para, void *parent);
int dtaudio_start (void *audio_priv);
int dtaudio_pause (void *audio_priv);
int dtaudio_resume (void *audio_priv);
int dtaudio_stop (void *audio_priv);
int dtaudio_release (void *audio_priv);
int64_t dtaudio_get_pts (void *audio_priv);
int dtaudio_drop (void *audio_priv, int64_t target_pts);
int64_t dtaudio_get_first_pts (void *audio_priv);
int dtaudio_get_state (void *audio_priv, dec_state_t * dec_state);
int dtaudio_get_out_closed (void *audio_priv);

#endif
