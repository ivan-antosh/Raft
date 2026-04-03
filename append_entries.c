#include <append_entries.h>
#include <stddef.h>
#include <types.h>

AppendResult AppendEntries(int term, int leaderId, int prevLogIndex, int prevLogTerm,
	LogEntry *entries, int numEntries, int leaderCommit) {
	AppendResult result;
	result.success = 0;
	result.term = 0;
	return result;
}
