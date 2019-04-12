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

#ifdef __APPLE__
# include <OpenAL/al.h>
# include <OpenAL/alc.h>
#else
# include <AL/al.h>
# include <AL/alc.h>
#endif

#define SAMPLING_BUFFER_SIZE (1024 * 32)
#define BUFFERS_NB 2

#if DEBUG

    static const char *get_error_name(ALenum r)
    {
#   define C(x) case x: return #x; break;
        switch (r) {
        C(AL_INVALID_NAME)
        C(AL_INVALID_ENUM)
        C(AL_INVALID_VALUE)
        C(AL_INVALID_OPERATION)
        C(AL_OUT_OF_MEMORY)
        default: return "unknown error";
        }
#   undef C
    }

#   define AL(x) do {                                       \
        x;                                                  \
        ALenum err_ = alGetError();                         \
        if (err_ != AL_NO_ERROR)                            \
            LOG_E("AL error: %s", get_error_name(err_));    \
            assert(err_ == AL_NO_ERROR);                    \
        } while (0);

#else // ! DEBUG
#   define AL(x) x
#endif

struct sound_backend {
    int format;
    int rate;
    ALuint          source;
    ALuint          buffers[BUFFERS_NB];
    int             buffers_start;  // Index of the first buffer
    int             buffers_count;  // Number of buffers in the queue
};

static struct {
    ALCdevice*  device;
    ALCcontext* context;
} global = {0};

sound_backend_t *sound_backend_create(sound_source_t *source)
{
    sound_backend_t *sound = calloc(1, sizeof(*sound));
    AL(alGenBuffers(BUFFERS_NB, sound->buffers));
    AL(alGenSources(1, &sound->source));
    sound->format = (source->channels == 1) ? AL_FORMAT_MONO16
                                            : AL_FORMAT_STEREO16;
    sound->rate = source->rate;
    return sound;
}

void sound_backend_delete(sound_backend_t *b)
{
    ALint i, nb;
    ALuint buff;
    AL(alSourceStop(b->source));

    AL(alGetSourcei(b->source, AL_BUFFERS_PROCESSED, &nb));
    assert(nb <= b->buffers_count);
    for (i = 0; i < nb; i++) {
        // If I don't use a temp variable for the buffer name, then I have
        // some strange bugs on iOS where alSourceUnqueueBuffers modifies the
        // values of the buffers!
        buff = b->buffers[b->buffers_start];
        AL(alSourceUnqueueBuffers(b->source, 1, &buff));
        b->buffers_start = (b->buffers_start + 1) % BUFFERS_NB;
        b->buffers_count--;
    }

    AL(alDeleteBuffers(BUFFERS_NB, b->buffers));
    AL(alDeleteSources(1, &b->source));
    free(b);
}

void sound_backend_init()
{
    assert(!global.device);
    assert(!global.context);
    global.device = alcOpenDevice(NULL);
    global.context = alcCreateContext(global.device, NULL);
    AL(alcMakeContextCurrent(global.context));
}

void sound_backend_release()
{
    alcMakeContextCurrent(NULL);
    alcDestroyContext(global.context);
    global.context = NULL;
    alcCloseDevice(global.device);
    global.device = NULL;
}

void sound_backend_stop_sound(sound_t *sound)
{
    AL(alSourceStop(sound->backend->source));
}

void sound_backend_play_sound(sound_t *sound, float volume, float pitch)
{
    assert(pitch >= 0.5 && pitch <= 2.0);
    AL(alSourcef(sound->backend->source, AL_GAIN,  volume));
    AL(alSourcef(sound->backend->source, AL_PITCH, pitch));
}

int sound_backend_iter_sound(sound_t *sound)
{
    ALint nb, i;
    int size;
    static char buffer[SAMPLING_BUFFER_SIZE];
    ALuint buff;
    sound_backend_t *b = sound->backend;

    assert(b->buffers_count <= BUFFERS_NB);
    AL(alGetSourcei(b->source, AL_BUFFERS_PROCESSED, &nb));
    assert(nb <= b->buffers_count);
    for (i = 0; i < nb; i++) {
        // If I don't use a temp variable for the buffer name, then I have
        // some strange bugs on iOS where alSourceUnqueueBuffers modifies the
        // values of the buffers!
        buff = b->buffers[b->buffers_start];
        AL(alSourceUnqueueBuffers(b->source, 1, &buff));
        b->buffers_start = (b->buffers_start + 1) % BUFFERS_NB;
        b->buffers_count--;
    }

    if (b->buffers_count == BUFFERS_NB)
        return 1;

    size = sound->source->read(sound->source, buffer, SAMPLING_BUFFER_SIZE);
    assert(size <= SAMPLING_BUFFER_SIZE);
    if (size == 0) {
        return b->buffers_count == 0 ? 0 : 1;
    }
    i = (b->buffers_start + b->buffers_count) % BUFFERS_NB;
    b->buffers_count++;
    AL(alBufferData(b->buffers[i], b->format, buffer, size, b->rate));
    AL(alSourceQueueBuffers(b->source, 1, &b->buffers[i]));
    if (b->buffers_count == 1) {
        AL(alSourcePlay(b->source));
    }
    return 1;
}

void sound_backend_iter(void)
{
}
