
#include <mpg123.h>
#include <alsa/asoundlib.h>

#pragma once

constexpr int BUFFER_SIZE = 8192;

class MP3wrapper {
public:
    bool init();
    bool open(const char* filename);
    void close();
    void scan();
    off_t length();
    int read(void *buffer, size_t *done);
    void seekAndReset(snd_pcm_t* pcm, off_t sample);
    off_t getFirstOffset();
    
    long getRate() { return rate; };
    int getChannels() { return channels; };
    int getEcoding() { return encoding; };

private:
    mpg123_handle* mh;
    long rate;
    int channels;
    int encoding;
};
