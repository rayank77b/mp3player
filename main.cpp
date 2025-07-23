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
#include <cxxopts.hpp>
#include "MP3wrapper.hpp"
#include "SNDwrapper.hpp"
#include "helper.hpp"

int main(int argc, char *argv[]) {
    cxxopts::Options options("mp3player", "play mp3 or mp4 files audio");

    options.add_options()
        ("f,from",   "Startzeit (default 0, begin)",   cxxopts::value<int>()->default_value("0"))
        ("t,to",     "Endzeit (default 0, end)",     cxxopts::value<int>()->default_value("0"))
        ("n,number", "Anzahl Durchläufe ", cxxopts::value<int>()->default_value("1"))
        ("x,velocity", "Geschwindigkeit erhöhen/erniedriegen ", cxxopts::value<float>()->default_value("1.0"))
        ("p,print", "Print Infos/Tags", cxxopts::value<bool>()->default_value("false"))
        ("h,help",   "Zeige diese Hilfe an")
        // positional argument "files"
        ("files",    "Eingabedateien (.mp3 oder .mp4)", cxxopts::value<std::vector<std::string>>());

    options.parse_positional({"files"});
    options.positional_help("d1.mp3 [d2.mp3 ... dN.mp4]");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help({""}) << "\n";
        return 0;
    }

    // eine datei soll angebeben werden
    if (!result.count("files")) {
        std::cerr << "Fehler: Keine Eingabedateien angegeben.\n"
                    << "Aufruf: app [Optionen] d1.mp3 [d2.mp3 ... dN.mp4]\n";
        return 1;
    }

    auto files = result["files"].as<std::vector<std::string>>();
    // Prüfung der Dateiendungen
    for (const auto& f : files) {
        if (!has_valid_extension(f)) {
            std::cerr << "Ungültige Dateiendung bei \"" << f << "\". "
                        << "Erlaubt sind nur .mp3 und .mp4\n";
            return 1;
        }
    }

    // Auslesen der weiteren Parameter
    int from   = result["from"].as<int>();
    int to     = result["to"].as<int>();
    int number = result["number"].as<int>();
    float faster = result["velocity"].as<float>();
    bool print_on = result["print"].as<bool>();

    if(from<0)  from = 0;
    if(to<0) to = 0;
    if(from>to) to = from + 10;    // falls falscheingaben, mehr als 10 Sek abspielen
    
    // Debug-Ausgabe der eingelesenen Werte
    std::cout << "Dateien:\n";
    for (const auto& f : files) {
        std::cout << "  - " << f << "\n";
    }
    std::string from_s = from<1?"begin":std::to_string(from);
    std::string to_s  = to<1?"end":std::to_string(to);
    std::cout << "Parameter:\n"
              << "  from   = " << from_s << "\n"
              << "  to     = " << to_s   << "\n"
              << "  number = " << number << "\n"
              << "  faster = " << faster << " x\n";

    const char* filename = files[0].c_str();

    if(print_on){
        for(auto filename : files)
            printTAG(filename.c_str());
    }

    MP3wrapper mp3;
    if(!mp3.init())
        return 1;
    if(!mp3.open(filename))
        return 1;
    
    // ALSA initialisieren
    SNDwrapper snd;
    if(!snd.init()) {
        mp3.close();
        return 1;
    }
    
    if (mp3.getEcoding() == MPG123_ENC_SIGNED_16) {
        snd.setFormat(SND_PCM_FORMAT_S16_LE);
    } else {
        std::cerr << "Nicht unterstuetztes Encoding" << std::endl;
        mp3.close();
        snd.close(0);
        return 1;
    }

    snd.setParameters(mp3);

    unsigned char buffer[BUFFER_SIZE];
    size_t done = 0;

    mp3.scan();
    off_t first_sampe_offset = mp3.getFirstOffset();
    off_t total_samples = mp3.length();
    if(first_sampe_offset<from*mp3.getRate()) first_sampe_offset = from*mp3.getRate();
    off_t current_sample = first_sampe_offset;

    std::atomic<bool> running(true);
    std::atomic<bool> paused(false);
    std::atomic<bool> restart(false);
    std::atomic<bool> alsa_paused(false);
    std::atomic<bool> forward_set(false);
    std::atomic<bool> backward_set(false);
    std::atomic<int> zeitgeber(0);

    zeitgeber = from;
    std::cout<<current_sample<<std::endl;
    mp3.seekAndReset( snd.getPCM(), current_sample);

    // Thread für Tastatureingaben
    std::thread inputThread([&]{
        setNonBlocking(true);
        std::cout << "\nSteuerung: p = Pause/Play, s = StartZero, f = Vorlauf 5s, r = Rücklauf 5s, q = Quit\n";
        while (running) {
            int c = getchar();
            if (c == EOF) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            c = tolower(c);
            if (c == 'p') {
                paused = !paused;
                if(paused)
                    printColor("Pause", RED);
                else
                    printColor("Play", GREEN);
            } else if (c == 's') {
                restart = true;
                printColor("Begining from Zero", RED);
            } else if (c == 'f') {
                if(!backward_set) forward_set = true;
            } else if (c == 'r') {
                if(!forward_set) backward_set = true;
            } else if (c == 'q') {
                running = false;
                printColor("Beenden", YELLOW );
            }
        }
        setNonBlocking(false);
    });

    std::thread timerThread([&]{
        while(running) {
            if(!paused)
                zeitgeber++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });

    // Wiedergabe Loop
    while (running) {
        if (paused) {
            if (!alsa_paused) {
                snd.pause(1);
                alsa_paused = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        } else if (alsa_paused) {
            snd.pause(0);
            alsa_paused = false;
        }

        if(forward_set) {
            off_t newpos = current_sample + mp3.getRate() * 5; // 5 Sekunden vor
            if (newpos > total_samples) newpos = total_samples;
            mp3.seekAndReset( snd.getPCM(), newpos);
            current_sample = newpos;
            printColor("Vorlauf 5s", YELLOW);
            if(zeitgeber<(total_samples/mp3.getRate()))  zeitgeber += 5;
            forward_set = false;
        }
        if(backward_set) { 
            off_t newpos = current_sample - mp3.getRate() * 5; // 6 Sekunden zurück
            std::cout<<std::endl<<"cur: "<<current_sample<<"  ";
            if (newpos < 0) newpos = first_sampe_offset;
            std::cout<<"new: "<<newpos;
            mp3.seekAndReset(snd.getPCM(), newpos);
            current_sample = newpos;
            printColor("Ruecklauf 5s", YELLOW);
            if(zeitgeber>=5) 
                zeitgeber -=5;
            else
                zeitgeber = 0;
            backward_set = false;
        }
        if(restart) {
            current_sample = first_sampe_offset;
            mp3.seekAndReset(snd.getPCM(), current_sample);
            printColor("Reset to 0", YELLOW);
            //std::cout<<"reset to "<<current_sample<<std::endl;
            zeitgeber = 0;
            restart = false;
        }

        int frames = done / (mp3.getChannels() * 2);

        // play until to
        if(current_sample>(to+12)*mp3.getRate()) {
            running = false;
        }

        int err = mp3.read(buffer, &done);
        if (err == MPG123_DONE || done == 0) {
            if(snd.is_playback_finished())
                break; // Ende der Datei
        } else if (err != MPG123_OK) {
            break;
        }

        // write to snd buffer x frames
        if(!snd.write(buffer, frames))
            break;

        // Fortschritt in Minuten:Sekunden, is very bugy!!!
        int total_sec = total_samples / mp3.getRate();
        int cur_sec = zeitgeber;

        std::cout << "\rFortschritt: " << current_sample<<" "
             << (cur_sec / 60) << ":" << (cur_sec % 60 < 10 ? "0" : "") 
             << (cur_sec % 60)
             << " / " << (total_sec / 60) << ":" << (total_sec % 60 < 10 ? "0" : "") 
             << (total_sec % 60) << "  "
             << std::flush;

        current_sample += frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    snd.close(1);

    running = false;
    inputThread.join();
    timerThread.join();

    printColor("Wiedergabe beendet.", YELLOW);

    snd.close(2);
    mp3.close();

    return 0;
}