#pragma once
#include <stdbool.h>

/* Initialize the multicast UDP logger. Must be called after Wi-Fi is up. */
bool log_udp_init(void);

/* printf-style; sends one packet per call to 224.0.0.123:9999. Cheap when
 * uninitialized (no-op). */
void log_udp_send(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Convenience: log both to UART and over UDP. */
#define LOG(...) do { printf(__VA_ARGS__); log_udp_send(__VA_ARGS__); } while (0)
