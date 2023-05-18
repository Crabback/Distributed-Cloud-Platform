#include "utils.h"

/**
 * Does this string start with a certain substring?
 */
bool starts_with(const string &haystack, const string &needle, bool case_sensitive) {
    if (case_sensitive)
        return haystack.rfind(needle, 0) == 0;
    // non-case-sensitive is a little trickier; make a lowercase copy of each and compare them
    string lower_str = string(haystack), lower_needle = string(needle);
    transform(haystack.begin(), haystack.end(), lower_str.begin(), ::tolower);
    transform(needle.begin(), needle.end(), lower_needle.begin(), ::tolower);
    return lower_str.rfind(lower_needle, 0) == 0;
}

/**
 * Does this string end with a certain substring?
 */
bool ends_with(const string &haystack, const string &needle, bool case_sensitive) {
    if (needle.length() > haystack.length()) return false;
    if (case_sensitive)
        return haystack.find(needle, haystack.length() - needle.length()) != string::npos;
    // non-case-sensitive is a little trickier; make a lowercase copy of each and compare them
    string lower_str = string(haystack), lower_needle = string(needle);
    transform(haystack.begin(), haystack.end(), lower_str.begin(), ::tolower);
    transform(needle.begin(), needle.end(), lower_needle.begin(), ::tolower);
    return lower_str.find(lower_needle, lower_str.length() - lower_needle.length()) != string::npos;
}

/**
 * Are these 2 strings equal, ignoring case?
 */
bool eq_case(const string &a, const string &b) {
    string la = string(a), lb = string(b);
    transform(a.begin(), a.end(), la.begin(), ::tolower);
    transform(b.begin(), b.end(), lb.begin(), ::tolower);
    return la == lb;
}

/**
 * Returns a copy of the input string with whitespace (sp, \r, \n) stripped from either end.
 */
string strip_ws(const string &str) {
    size_t start = str.find_first_not_of(" \r\n");
    if (start == string::npos)  // string is all whitespace
        return string("");
    size_t end = str.find_last_not_of(" \r\n");
    size_t count = end - start + 1;
    return str.substr(start, count);
}

/**
 * Get the token (sequence of non-ws chars) starting at *start* in *line*.
 */
string get_token(const string &line, size_t start, int *offset) {
    size_t next_ws = line.find_first_of(" \r\n", start);
    if (offset) *offset = next_ws;
    return line.substr(start, next_ws - start);
}

/**
 * Like printf, but prepends the current UTC time.
 */
void logf(const char *fmt, ...) {
    // prepend the log info
    struct timespec t;
    if (clock_gettime(CLOCK_REALTIME, &t) == -1) {
        perror("gettime");
    } else {
        struct tm *tm = gmtime(&t.tv_sec);
        fprintf(stdout, "%02u:%02u:%02u.%06lu ", tm->tm_hour, tm->tm_min, tm->tm_sec, t.tv_nsec / 1000);
    }
    // pass on to vfprintf
    va_list argptr;
    va_start(argptr, fmt);
    vfprintf(stdout, fmt, argptr);
    va_end(argptr);
}
