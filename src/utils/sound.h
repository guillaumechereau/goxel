#include <stdbool.h>

void sound_init(void);
void sound_register(const char *name, const char *wav_data);
void sound_play(const char *name, float volume, float pitch);
void sound_iter(void);

bool sound_is_enabled(void);
void sound_set_enabled(bool v);
