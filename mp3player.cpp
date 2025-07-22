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
#include <cxxopts.hpp>

using namespace std;

#define BUFFER_SIZE 8192

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

void seekAndReset(mpg123_handle* mh, snd_pcm_t* pcm, off_t sample) {
    mpg123_seek(mh, sample, SEEK_SET);
    snd_pcm_drop(pcm);
    snd_pcm_prepare(pcm);
}

void printTAG(const char *filename) {
    TagLib::FileRef f(filename);
    if (!f.isNull() && f.tag()) {
        TagLib::Tag *tag = f.tag();
        std::cout << "=================================================\n";
        std::cout << "MP3 Datei: " << filename << endl;
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

off_t getFirstOffset(mpg123_handle *mh) {
    off_t* offsets;
	off_t step;
	size_t fill;

    mpg123_scan(mh);
    mpg123_index(mh, &offsets, &step, &fill);

    return offsets[0];
}

bool is_playback_finished(snd_pcm_t* pcm_handle) {
    snd_pcm_sframes_t delay;
    if (snd_pcm_delay(pcm_handle, &delay) < 0) {
        std::cerr << "snd_pcm_delay fehlgeschlagen\n";
        return false;
    }
    cout<<"  delay " << delay<<"  "<<flush;
    if(delay<1024) {
        cout<<" delayed end ....\n";
        return true;
    } else
        return false;
}

// Hilfsfunktion zur Prüfung der Dateiendung
bool has_valid_extension(const std::string& filename) {
    auto pos = filename.rfind('.');
    if (pos == std::string::npos) return false;
    std::string ext = filename.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".mp3" || ext == ".mp4");
}

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

    cout << "Sample rate: " << rate << " Hz" << endl;
    cout << "Kanaele: " << channels << endl;

    // ALSA initialisieren
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    if (snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        cerr << "Fehler beim Öffnen des ALSA-Geräts" << endl;
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
        return 1;
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_format_t format;
    if (encoding == MPG123_ENC_SIGNED_16) {
        format = SND_PCM_FORMAT_S16_LE;
    } else {
        cerr << "Nicht unterstuetztes Encoding" << endl;
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

    mpg123_scan(mh);
    off_t first_sampe_offset = getFirstOffset(mh);
    off_t total_samples = mpg123_length(mh);
    off_t current_sample = first_sampe_offset;

    atomic<bool> running(true);
    atomic<bool> paused(false);
    atomic<bool> restart(false);
    atomic<bool> alsa_paused(false);
    atomic<bool> forward_set(false);
    atomic<bool> backward_set(false);
    atomic<int> zeitgeber(0);

    // Thread für Tastatureingaben
    thread inputThread([&]{
        setNonBlocking(true);
        cout << "\nSteuerung: p = Pause/Play, s = StartZero, f = Vorlauf 5s, r = Rücklauf 5s, q = Quit\n";
        while (running) {
            int c = getchar();
            if (c == EOF) {
                this_thread::sleep_for(chrono::milliseconds(100));
                continue;
            }
            c = tolower(c);
            if (c == 'p') {
                paused = !paused;
                cout << (paused ? "\nPause\n" : "\nPlay\n");
            } else if (c == 's') {
                restart = true;
                cout << "\nBegining from Zero\n";
            } else if (c == 'f') {
                if(!backward_set) forward_set = true;
            } else if (c == 'r') {
                if(!forward_set) backward_set = true;
            } else if (c == 'q') {
                running = false;
                cout << "\nBeenden\n";
            }
        }
        setNonBlocking(false);
    });

    thread timerThread([&]{
        while(running) {
            if(!paused)
                zeitgeber++;
            this_thread::sleep_for(chrono::milliseconds(1000));
        }
    });

    // Wiedergabe Loop
    while (running) {
        if (paused) {
            if (!alsa_paused) {
                snd_pcm_pause(pcm_handle, 1);  // ALSA pausieren
                alsa_paused = true;
            }
            this_thread::sleep_for(chrono::milliseconds(100));
            continue;
        } else if (alsa_paused) {
            snd_pcm_pause(pcm_handle, 0);  // ALSA fortsetzen
            alsa_paused = false;
        }

        if(forward_set) {
            off_t newpos = current_sample + rate * 5; // 5 Sekunden vor
            if (newpos > total_samples) newpos = total_samples;
            seekAndReset(mh, pcm_handle, newpos);
            current_sample = newpos;
            cout << "\nVorlauf 5s\n";
            if(zeitgeber<(total_samples/rate))  zeitgeber += 5;
            forward_set = false;
        }
        if(backward_set) { 
            off_t newpos = current_sample - rate * 5; // 6 Sekunden zurück
            cout<<endl<<"cur: "<<current_sample<<"  ";
            if (newpos < 0) newpos = first_sampe_offset;
            cout<<"new: "<<newpos;
            seekAndReset(mh, pcm_handle, newpos);
            current_sample = newpos;
            cout<<"  cur: "<<current_sample<<"  ";
            cout << "         Ruecklauf 5s\n";
            if(zeitgeber>=5) 
                zeitgeber -=5;
            else
                zeitgeber = 0;
            backward_set = false;
        }
        if(restart) {
            current_sample = first_sampe_offset;
            seekAndReset(mh, pcm_handle, current_sample);
            cout<<"reset to "<<current_sample<<endl;
            zeitgeber = 0;
            restart = false;
        }


        int err = mpg123_read(mh, buffer, BUFFER_SIZE, &done);
        if (err == MPG123_DONE || done == 0) {
            if(is_playback_finished(pcm_handle))
                break; // Ende der Datei
        } else if (err != MPG123_OK) {
            cerr << "mpg123_read error: " << mpg123_strerror(mh) << endl;
            break;
        }

        int frames = done / (channels * 2);
        //cout<<"\n done: "<<done<<"  frames: "<<frames<<" \n"<<flush;
        int err2 = snd_pcm_writei(pcm_handle, buffer, frames);
        if (err2 == -EPIPE) {
            snd_pcm_prepare(pcm_handle);
        } else if (err2 < 0) {
            cerr << "ALSA write error: " << snd_strerror(err2) << endl;
            break;
        }

        // Fortschritt in Minuten:Sekunden
        int total_sec = total_samples / rate;
        int cur_sec = zeitgeber;
        

        cout << "\rFortschritt: " << current_sample<<" "
             << (cur_sec / 60) << ":" << (cur_sec % 60 < 10 ? "0" : "") 
             << (cur_sec % 60)
             << " / " << (total_sec / 60) << ":" << (total_sec % 60 < 10 ? "0" : "") 
             << (total_sec % 60) << "  "
             << flush;

        current_sample += frames;
        this_thread::sleep_for(chrono::milliseconds(20));
    }

    snd_pcm_drop(pcm_handle);
    snd_pcm_prepare(pcm_handle);

    running = false;
    inputThread.join();
    timerThread.join();

    cout << endl << "Wiedergabe beendet." << endl;

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();

    return 0;
}