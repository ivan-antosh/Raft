#ifndef APPEND_ENTRIES_H
#define APPEND_ENTRIES_H 

typedef struct {
	int term; /* current term */
	int success; /* 0 fail, 1 success */
} AppendResult;

AppendResult *AppendEntries(int term, int leaderId, int prevLogIndex, int prevLogTerm,
	char **entries, int leaderCommit);

#endif
