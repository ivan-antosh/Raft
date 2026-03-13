#include <stdio.h>
#include <stdlib.h>
#include <append_entries.h>
#include <request_vote.h>
#include <types.h>

/* persistent on all servers */
/* lastest term server has seen */
int currentTerm = 0;
/* candidateId that received vote in current term, NULL if none */
int *votedFor = NULL;
/* log entries, command for state machine and term when entry received by leader */
LogEntry *logEntries; /* first index is 1 */

/* volatile on all servers */
/* index of highest log entry known to be committed */
int commitIndex = 0;
/* index of highest log entry applied to state machined */
int lastApplied = 0;

/* volatile on leaders (reinitialized after election)*/
/* index of next log entry to send for each server */
int nextIndex[NUM_SERVERS]; /* init to leader last log index + 1 */
/* index of highest log entry known to be replicated on each server */
int matchIndex[NUM_SERVERS]; /* init to 0 */

int main() {
	return 0;
}
