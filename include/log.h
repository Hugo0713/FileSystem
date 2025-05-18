#include <err.h>
#include <stdio.h>

#define COLOR_ERROR "\033[31m[Error]\033[0m "

extern FILE *log_file;

inline static void log_init(const char *fname) {
    log_file = fopen(fname, "w");
    if (log_file == NULL) err(1, COLOR_ERROR "fopen %s", fname);
}

inline static void log_close(void) { fclose(log_file); }

#define Log(format, ...)                                         \
    do {                                                         \
        fprintf(log_file, "[INFO] " format "\n", ##__VA_ARGS__); \
        fflush(log_file);                                        \
    } while (0)

#define Warn(format, ...)                                        \
    do {                                                         \
        fprintf(log_file, "[WARN] " format "\n", ##__VA_ARGS__); \
        fflush(log_file);                                        \
    } while (0)

#define Error(format, ...)                                        \
    do {                                                          \
        fprintf(log_file, "[ERROR] " format "\n", ##__VA_ARGS__); \
        fflush(log_file);                                         \
    } while (0)

#ifdef DEBUG
#define Debug(format, ...)                                        \
    do {                                                          \
        fprintf(log_file, "[DEBUG] " format "\n", ##__VA_ARGS__); \
        fflush(log_file);                                         \
    } while (0)
#else
#define Debug(format, ...)
#endif
