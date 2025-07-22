# Compiler and flags
CXX       = g++-14
CXXFLAGS  = -Wall -O2 -pthread
LDFLAGS   = -lmpg123 -lasound -ltag

# Source files and target
SRC       = helper.cpp MP3wrapper.cpp main.cpp
OBJ       = $(SRC:.cpp=.o)
TARGET    = mp3player

.PHONY: all clean

# Default target
all: $(TARGET)

# Link object files into the final binary
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile .cpp files to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Remove build artifacts
clean:
	rm -f $(OBJ) $(TARGET)