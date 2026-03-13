#ifndef REQUEST_VOTE_H
#define REQUEST_VOTE_H

typedef struct {
	int term; /* current term */
	int voteGranted; /* 0 fail 1 success */
} RequestResult;

RequestResult *RequestVote(int term, int candidateId, int lastlogIndex, int lastLogTerm);

#endif
