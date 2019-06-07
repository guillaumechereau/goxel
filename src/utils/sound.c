/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Basic sound support.
 *
 * The code is probably overcomplicated, because this is adapted from
 * some old code I used for video games, that supported much more features,
 * like music, effects, ogg files...
 *
 * This is disabled by default.
 */

#ifndef SOUND_H
#define SOUND_H

#include "sound.h"

#include "uthash.h"

#include <assert.h>

#ifdef SOUND

// Abstraction used to represent any kind of sound sources: wav, sfxr, mod.
typedef struct sound_source {
    void    *data;
    int     rate, channels, size;
    int     (*read)(struct sound_source *source, void *data, int size);
    void    (*delete)(struct sound_source *source);
    void    (*reset)(struct sound_source *source);
} sound_source_t;

typedef struct sound_backend sound_backend_t;

typedef struct sound {
    UT_hash_handle  hh;
    int state;
    sound_source_t  *source;
    sound_backend_t *backend;
} sound_t;

static sound_t *g_sounds = NULL;
static bool g_enabled = true;


// Functions defined by the backend.
void sound_backend_init(void);
sound_backend_t *sound_backend_create(sound_source_t *source);
void sound_backend_play_sound(sound_t *sound, float volume, float pitch);
void sound_backend_stop_sound(sound_t *sound);
int sound_backend_iter_sound(sound_t *sound);
void sound_backend_iter(void);


void sound_init(void)
{
    sound_backend_init();
}

bool sound_is_enabled(void)
{
    return g_enabled;
}

void sound_set_enabled(bool v)
{
    g_enabled = v;
}


typedef struct {
    void    *buffer;
    int     size;
    int     pos;
} wav_t;

static int wav_read(sound_source_t *source, void *out, int size)
{
    wav_t *wav = source->data;
    if (wav->size - wav->pos < size)
        size = wav->size - wav->pos;
    if (out)
        memcpy(out, wav->buffer + wav->pos, size);
    wav->pos += size;
    return size;
}

static void wav_reset(sound_source_t *source)
{
    wav_t *wav = source->data;
    wav->pos = 0;
}

static void wav_delete(sound_source_t *source)
{
    wav_t *wav = source->data;
    free(wav->buffer);
    free(wav);
    free(source);
}

static sound_source_t *wav_create(const char *data)
{
    sound_source_t *ret = calloc(1, sizeof(*ret));
    wav_t *wav = calloc(1, sizeof(*wav));
    assert(data);
    ret->size = wav->size = *(uint32_t*)(data + 40);
    ret->rate = *(uint32_t*)(data + 24);
    ret->channels = *(uint16_t*)(data + 22);
    wav->buffer = malloc(wav->size);
    memcpy(wav->buffer, data + 44, wav->size);

    ret->data = wav;
    ret->read = wav_read;
    ret->reset = wav_reset;
    ret->delete = wav_delete;
    return ret;
}

void sound_register(const char *name, const char *wav)
{
    sound_t *sound;

    HASH_FIND_STR(g_sounds, name, sound);
    assert(!sound);

    sound = calloc(1, sizeof(*sound));
    sound->source = wav_create(wav);
    assert(sound->source);
    sound->backend = sound_backend_create(sound->source);
    HASH_ADD_KEYPTR(hh, g_sounds, name, strlen(name), sound);
}

void sound_play(const char *name, float volume, float pitch)
{
    sound_t *sound;
    if (!g_enabled) return;
    HASH_FIND_STR(g_sounds, name, sound);
    assert(sound);
    sound->source->reset(sound->source);
    sound_backend_stop_sound(sound);
    sound->state = 1;
    sound_backend_play_sound(sound, volume, pitch);
}

void sound_iter(void)
{
    sound_t *sound, *tmp;
    HASH_ITER(hh, g_sounds, sound, tmp) {
        sound->state = sound_backend_iter_sound(sound);
    }
    sound_backend_iter();
}

// For the moment the only backend we support is OpenAL.
#include "sound_openal.inl"

#else
// Dummy API when we compile without sound support.

void sound_init(void) {}
void sound_register(const char *name, const char *wav_data) {}
void sound_play(const char *name, float volume, float pitch) {}
void sound_iter(void) {}
bool sound_is_enabled(void) { return false; }
void sound_set_enabled(bool v) {}

#endif

#endif // SOUND_H
