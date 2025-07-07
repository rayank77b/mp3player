#include <iostream>
#include <mpg123.h>
#include <alsa/asoundlib.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

using namespace std;

#define BUFFER_SIZE 8192

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <mp3-file>" << endl;
        return 1;
    }

    const char* filename = argv[1];

    // mpg123 initialisieren
    if (mpg123_init() != MPG123_OK) {
        cerr << "Failed to initialize mpg123" << endl;
        return 1;
    }

    mpg123_handle *mh = mpg123_new(nullptr, nullptr);
    if (!mh) {
        cerr << "Failed to create mpg123 handle" << endl;
        mpg123_exit();
        return 1;
    }

    if (mpg123_open(mh, filename) != MPG123_OK) {
        cerr << "Failed to open file: " << filename << endl;
        mpg123_delete(mh);
        mpg123_exit();
        return 1;
    }

    long rate;
    int channels, encoding;
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
        cerr << "Failed to get format from file" << endl;
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
        return 1;
    }

    cout << "Playing: " << filename << endl;
    cout << "Sample rate: " << rate << " Hz" << endl;
    cout << "Channels: " << channels << endl;

    // ALSA initialisieren
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_format_t format;
    if (encoding == MPG123_ENC_SIGNED_16) {
        format = SND_PCM_FORMAT_S16_LE;
    } else {
        cerr << "Unsupported encoding format" << endl;
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
        snd_pcm_hw_params_free(params);
        snd_pcm_close(pcm_handle);
        return 1;
    }

    snd_pcm_hw_params_set_format(pcm_handle, params, format);
    snd_pcm_hw_params_set_channels(pcm_handle, params, channels);
    snd_pcm_hw_params_set_rate(pcm_handle, params, rate, 0);
    snd_pcm_hw_params(pcm_handle, params);
    snd_pcm_hw_params_free(params);
    snd_pcm_prepare(pcm_handle);

    unsigned char buffer[BUFFER_SIZE];
    size_t done = 0;
    off_t total_samples = mpg123_length(mh);
    off_t current_sample = 0;

    // Abspielen
    while (mpg123_read(mh, buffer, BUFFER_SIZE, &done) == MPG123_OK) {
        if (done == 0) break;

        int frames = done / (channels * 2); // 2 bytes per sample for S16_LE
        int err = snd_pcm_writei(pcm_handle, buffer, frames);
        if (err == -EPIPE) {
            snd_pcm_prepare(pcm_handle);
        } else if (err < 0) {
            cerr << "ALSA write error: " << snd_strerror(err) << endl;
            break;
        }

        current_sample += frames;
        double progress = 100.0 * current_sample / total_samples;
        cout << "\rProgress: " << int(progress) << "%" << flush;
    }

    cout << endl << "Playback finished." << endl;

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();

    return 0;
}