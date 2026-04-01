#ifndef REQUEST_VOTE_H
#define REQUEST_VOTE_H

#include <types.h>

/* TODO: */
typedef struct {
	int sockfd;
	RPCMsg msg;
} RequestVoteArgs;

typedef struct {
	int term; /* current term */
	int voteGranted; /* 0 fail 1 success */
} RequestResult;

/* TODO: */
void *RequestVoteThread(void *ptr);

/* TODO: */
RequestResult *RequestVote(int sockfd, int rpcType, int term, int candidateId, int lastLogIndex, int lastLogTerm);

#endif
