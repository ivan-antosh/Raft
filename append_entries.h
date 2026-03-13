#ifndef APPEND_ENTRIES_H
#define APPEND_ENTRIES_H 

#include <types.h>

typedef struct {
	int term; /* current term */
	int success; /* 0 fail, 1 success */
} AppendResult;

AppendResult *AppendEntries(int term, int leaderId, int prevLogIndex, int prevLogTerm,
	Command *entries, int leaderCommit);

#endif
