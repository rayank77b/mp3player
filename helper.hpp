#pragma once

// Termios Setup für nicht-blockierendes Lesen der Tastatur
void setNonBlocking(bool enable);

void printTAG(const char *filename);

// Hilfsfunktion zur Prüfung der Dateiendung
bool has_valid_extension(const std::string& filename);

void debugme(const std::string& info);