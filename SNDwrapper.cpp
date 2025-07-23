#include <mpg123.h>
#include <alsa/asoundlib.h>
#include <iostream>

#include "SNDwrapper.hpp"

bool SNDwrapper::init() {
    if (snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        std::cerr << "Fehler beim Öffnen des ALSA-Geräts" << std::endl;
        return false;
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    return true;
}

void SNDwrapper::close(int c) {
    if(c==0){
        snd_pcm_hw_params_free(params);
        snd_pcm_close(pcm_handle);
    } else if(c==1){
        snd_pcm_drop(pcm_handle);
        snd_pcm_prepare(pcm_handle);
    } else if(c==2){
        snd_pcm_drain(pcm_handle);
        snd_pcm_close(pcm_handle);
    }
}

void SNDwrapper::setParameters(MP3wrapper& mp3) {
    snd_pcm_hw_params_set_format(pcm_handle, params, format);
    snd_pcm_hw_params_set_channels(pcm_handle, params, mp3.getChannels());
    snd_pcm_hw_params_set_rate(pcm_handle, params, mp3.getRate(), 0);
    snd_pcm_hw_params(pcm_handle, params);
    snd_pcm_hw_params_free(params);
    snd_pcm_prepare(pcm_handle);
}

bool SNDwrapper::is_playback_finished() {
    snd_pcm_sframes_t delay;
    if (snd_pcm_delay(pcm_handle, &delay) < 0) {
        std::cerr << "snd_pcm_delay fehlgeschlagen\n";
        return false;
    }
    std::cout<<"  delay " << delay<<"  "<<std::flush;
    if(delay<1024) {
        std::cout<<" delayed end ....\n";
        return true;
    } else
        return false;
}

bool SNDwrapper::write(unsigned char* buffer, int frames) {
    int err = snd_pcm_writei(pcm_handle, buffer, frames);
    if (err == -EPIPE) {
        snd_pcm_prepare(pcm_handle);
    } else if (err < 0) {
        std::cerr << "ALSA write error: " << snd_strerror(err) << std::endl;
        return false;;
    }
    return true;
}