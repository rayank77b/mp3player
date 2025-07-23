#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <mpg123.h>
#include <alsa/asoundlib.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include "MP3wrapper.hpp"

bool MP3wrapper::init() {
    mh = mpg123_new(nullptr, nullptr);
    if (!mh) {
        std::cerr << "Failed to create mpg123 handle" << std::endl;
        mpg123_exit();
        return false;
    }
    // mpg123 initialisieren
    if (mpg123_init() != MPG123_OK) {
        std::cerr << "Failed to initialize mpg123" << std::endl;
        return false;
    }

    return true;
}

bool MP3wrapper::open(const char* filename) {
    if (mpg123_open(mh, filename) != MPG123_OK) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        mpg123_delete(mh);
        mpg123_exit();
        return false;
    }
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
        std::cerr << "Failed to get format from file" << std::endl;
        close();
        return false;
    }

    std::cout << "Sample rate: " << rate << " Hz" << std::endl;
    std::cout << "Kanaele: " << channels << std::endl;
    return true;
}

void MP3wrapper::close() {
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
}

void MP3wrapper::scan() {
    mpg123_scan(mh);
}

off_t MP3wrapper::length() {
    return mpg123_length(mh);
}

int MP3wrapper::read(void *buffer, size_t *done) {
    int err = mpg123_read(mh, buffer, BUFFER_SIZE, done);
    //if (err != MPG123_OK) 
    //    std::cerr << "mpg123_read error: "<<err<<" "<<*done<<" "<< mpg123_strerror(mh) << std::endl;
    return err;
}

void MP3wrapper::seekAndReset(snd_pcm_t* pcm, off_t sample) {
    mpg123_seek(mh, sample, SEEK_SET);
    snd_pcm_drop(pcm);
    snd_pcm_prepare(pcm);
}

off_t MP3wrapper::getFirstOffset() {
    off_t* offsets;
	off_t step;
	size_t fill;

    mpg123_scan(mh);
    mpg123_index(mh, &offsets, &step, &fill);

    return offsets[0];
}