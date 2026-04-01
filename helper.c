#include <types.h>
#include <stddef.h>

#include <helper.h>

char *serializeLogEntry(const LogEntry *entry, size_t *outLen) {
    return NULL;
}
char *serializeLogEntries(const LogEntry *entry, size_t *outLen, int numEntries) {
    return NULL;
}

LogEntry *deserializeLogEntry(const char *buf, size_t bufLen) {
    return NULL;
}

LogEntry *deserializeLogEntries(const char *buf, size_t bufLen, int numEntries) {
    return NULL;
}