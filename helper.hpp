#pragma once

constexpr const char* RESET  = "\033[0m";
constexpr const char* RED    = "\033[31m";
constexpr const char* GREEN  = "\033[32m";
constexpr const char* YELLOW  = "\033[33m";

void printColor(std::string message, const char *c);

// Termios Setup für nicht-blockierendes Lesen der Tastatur
void setNonBlocking(bool enable);

void printTAG(const char *filename);

// Hilfsfunktion zur Prüfung der Dateiendung
bool has_valid_extension(const std::string& filename);

void debugme(const std::string& info);