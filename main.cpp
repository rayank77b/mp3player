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

    // mpg123 initialisieren
    if (mpg123_init() != MPG123_OK) {
        std::cerr << "Failed to initialize mpg123" << std::endl;
        return 1;
    }

    MP3wrapper mp3;
    if(!mp3.init())
        return 1;
    if(!mp3.open(filename))
        return 1;
    
    // ALSA initialisieren
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    if (snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        std::cerr << "Fehler beim Öffnen des ALSA-Geräts" << std::endl;
        mp3.close();
        return 1;
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_format_t format;
    if (mp3.getEcoding() == MPG123_ENC_SIGNED_16) {
        format = SND_PCM_FORMAT_S16_LE;
    } else {
        std::cerr << "Nicht unterstuetztes Encoding" << std::endl;
        mp3.close();
        snd_pcm_hw_params_free(params);
        snd_pcm_close(pcm_handle);
        return 1;
    }

    snd_pcm_hw_params_set_format(pcm_handle, params, format);
    snd_pcm_hw_params_set_channels(pcm_handle, params, mp3.getChannels());
    snd_pcm_hw_params_set_rate(pcm_handle, params, mp3.getRate(), 0);
    snd_pcm_hw_params(pcm_handle, params);
    snd_pcm_hw_params_free(params);
    snd_pcm_prepare(pcm_handle);

    unsigned char buffer[BUFFER_SIZE];
    size_t done = 0;

    mp3.scan();
    off_t first_sampe_offset = mp3.getFirstOffset();
    off_t total_samples = mp3.length();
    off_t current_sample = first_sampe_offset;

    std::atomic<bool> running(true);
    std::atomic<bool> paused(false);
    std::atomic<bool> restart(false);
    std::atomic<bool> alsa_paused(false);
    std::atomic<bool> forward_set(false);
    std::atomic<bool> backward_set(false);
    std::atomic<int> zeitgeber(0);

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
                std::cout << (paused ? "\nPause\n" : "\nPlay\n");
            } else if (c == 's') {
                restart = true;
                std::cout << "\nBegining from Zero\n";
            } else if (c == 'f') {
                if(!backward_set) forward_set = true;
            } else if (c == 'r') {
                if(!forward_set) backward_set = true;
            } else if (c == 'q') {
                running = false;
                std::cout << "\nBeenden\n";
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
                snd_pcm_pause(pcm_handle, 1);  // ALSA pausieren
                alsa_paused = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        } else if (alsa_paused) {
            snd_pcm_pause(pcm_handle, 0);  // ALSA fortsetzen
            alsa_paused = false;
        }

        if(forward_set) {
            off_t newpos = current_sample + mp3.getRate() * 5; // 5 Sekunden vor
            if (newpos > total_samples) newpos = total_samples;
            mp3.seekAndReset( pcm_handle, newpos);
            current_sample = newpos;
            std::cout << "\nVorlauf 5s\n";
            if(zeitgeber<(total_samples/mp3.getRate()))  zeitgeber += 5;
            forward_set = false;
        }
        if(backward_set) { 
            off_t newpos = current_sample - mp3.getRate() * 5; // 6 Sekunden zurück
            std::cout<<std::endl<<"cur: "<<current_sample<<"  ";
            if (newpos < 0) newpos = first_sampe_offset;
            std::cout<<"new: "<<newpos;
            mp3.seekAndReset(pcm_handle, newpos);
            current_sample = newpos;
            std::cout<<"  cur: "<<current_sample<<"  ";
            std::cout << "         Ruecklauf 5s\n";
            if(zeitgeber>=5) 
                zeitgeber -=5;
            else
                zeitgeber = 0;
            backward_set = false;
        }
        if(restart) {
            current_sample = first_sampe_offset;
            mp3.seekAndReset(pcm_handle, current_sample);
            std::cout<<"reset to "<<current_sample<<std::endl;
            zeitgeber = 0;
            restart = false;
        }

        int err = mp3.read(buffer, &done);
        if (err == MPG123_DONE || done == 0) {
            if(is_playback_finished(pcm_handle))
                break; // Ende der Datei
        } else if (err != MPG123_OK) {
            break;
        }

        int frames = done / (mp3.getChannels() * 2);
        //cout<<"\n done: "<<done<<"  frames: "<<frames<<" \n"<<flush;
        int err2 = snd_pcm_writei(pcm_handle, buffer, frames);
        if (err2 == -EPIPE) {
            snd_pcm_prepare(pcm_handle);
        } else if (err2 < 0) {
            std::cerr << "ALSA write error: " << snd_strerror(err2) << std::endl;
            break;
        }

        // Fortschritt in Minuten:Sekunden
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

    snd_pcm_drop(pcm_handle);
    snd_pcm_prepare(pcm_handle);

    running = false;
    inputThread.join();
    timerThread.join();

    std::cout << std::endl << "Wiedergabe beendet." << std::endl;

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    mp3.close();

    return 0;
}