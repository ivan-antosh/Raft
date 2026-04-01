#ifndef HELPER_H
#define HELPER_H

#include <types.h>
#include <stddef.h>

char *serializeLogEntry(const LogEntry *entry, size_t *outLen);
char *serializeLogEntries(const LogEntry *entry, size_t *outLen, int numEntries);

LogEntry *deserializeLogEntry(const char *buf, size_t bufLen);
LogEntry *deserializeLogEntries(const char *buf, size_t bufLen, int numEntries);

#endif