CXX = g++-14
CXXFLAGS = -Wall -O2  -pthread
LIBS = -lmpg123 -lasound -ltag

all: mp3player getmp3info

mp3player: mp3player.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

mp3player_control: mp3player_control.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

getmp3info: getmp3info.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f mp3player mp3player_control