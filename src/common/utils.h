#pragma once

#include <string>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cstdarg>

using namespace std;

#define DEBUG 1

bool starts_with(const string &haystack, const string &needle, bool case_sensitive = true);
bool ends_with(const string &haystack, const string &needle, bool case_sensitive = true);
bool eq_case(const string &a, const string &b);
string strip_ws(const string &str);
string get_token(const string &line, size_t start = 0, int *offset = nullptr);
void logf(const char *fmt, ...);
