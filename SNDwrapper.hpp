#include <mpg123.h>
#include <alsa/asoundlib.h>

#include "MP3wrapper.hpp"

#pragma once

class SNDwrapper {
public:
    bool init();
    void close(int c);
    void setParameters(MP3wrapper& mp3);
    bool is_playback_finished();
    bool write(unsigned char* buffer, int frames); 

    void pause(int enable) { snd_pcm_pause(pcm_handle, enable); }; // ALSA pausieren}

    void setFormat(snd_pcm_format_t f) { format=f; };
    snd_pcm_format_t getFormat() { return format; };
    snd_pcm_t* getPCM() { return pcm_handle; };
    
private:
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_format_t format;
};