#ifndef UTILS_H
#define UTILS_H
#include <cstdio>
#include <string>

// Function to print a hexdump of a buffer
void hexdump(uint32_t *buf, int len);

// Function to trim whitespace from the start of a string
static inline void ltrim(std::string &s);

// Function to trim whitespace from the end of a string
static inline void rtrim(std::string &s);

// Function to trim whitespace from both ends of a string
void trim(std::string &s);

#endif // UTILS_H