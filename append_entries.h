#ifndef APPEND_ENTRIES_H
#define APPEND_ENTRIES_H 

#include <types.h>

int AppendEntries(int sockfd, int term, int leaderId, int prevLogIndex, int prevLogTerm,
	LogEntry *entries, int numEntries, int leaderCommit);

#endif
