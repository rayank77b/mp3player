#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <mpg123.h>
#include <alsa/asoundlib.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <termios.h>
#include <fcntl.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include "helper.hpp"

// Termios Setup für nicht-blockierendes Lesen der Tastatur
void setNonBlocking(bool enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO); // non-canonical, no echo
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, 0);
    }
}

void printTAG(const char *filename) {
    TagLib::FileRef f(filename);
    if (!f.isNull() && f.tag()) {
        TagLib::Tag *tag = f.tag();
        std::cout << "=================================================\n";
        std::cout << "MP3 Datei: " << filename << std::endl;
        std::cout << "Titel:   " << tag->title() << std::endl;
        std::cout << "Kuenstler:" << tag->artist() << std::endl;
        std::cout << "Album:   " << tag->album() << std::endl;
        std::cout << "Jahr:    " << tag->year() << std::endl;
        std::cout << "Track:   " << tag->track() << std::endl;
        std::cout << "Genre:   " << tag->genre() << std::endl;
    } else {
        std::cout << "Keine Tags gefunden oder Datei ungueltig." << std::endl;
    }
}

// Hilfsfunktion zur Prüfung der Dateiendung
bool has_valid_extension(const std::string& filename) {
    auto pos = filename.rfind('.');
    if (pos == std::string::npos) return false;
    std::string ext = filename.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".mp3" || ext == ".mp4");
}

void debugme(const std::string& info) {
    std::cerr << "Debug: "<<info<<std::endl;
}