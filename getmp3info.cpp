#include <iostream>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <mpg123.h>

using namespace std;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <mp3-file>" << std::endl;
        return 1;
    }

    TagLib::FileRef f(argv[1]);
    if (!f.isNull() && f.tag()) {
        TagLib::Tag *tag = f.tag();

        std::cout << "Titel:   " << tag->title() << std::endl;
        std::cout << "Künstler:" << tag->artist() << std::endl;
        std::cout << "Album:   " << tag->album() << std::endl;
        std::cout << "Jahr:    " << tag->year() << std::endl;
        std::cout << "Track:   " << tag->track() << std::endl;
        std::cout << "Genre:   " << tag->genre() << std::endl;
    } else {
        std::cout << "Keine Tags gefunden oder Datei ungültig." << std::endl;
    }

    mpg123_handle *mh;
    off_t* offsets;
	off_t step;
	size_t fill, i;

    mpg123_init();
    mh = mpg123_new(NULL, NULL);
    mpg123_param(mh, MPG123_RESYNC_LIMIT, -1, 0);
	mpg123_param(mh, MPG123_INDEX_SIZE, -1, 0);
	mpg123_open(mh, argv[1]);
	mpg123_scan(mh);
    mpg123_index(mh, &offsets, &step, &fill);

    for(i=0; i<10;i++) {
		cout<<"Frame number: "<<(intmax_t)(i * step)
            <<": file offset "<<(intmax_t)offsets[i]
            <<endl;  
	}
    
    cout<<"first offset: "<<offsets[0]<<endl;
    cout<<"sec: "<<offsets[0]/44100<<endl;
	mpg123_close(mh);
	mpg123_delete(mh);

    return 0;
}