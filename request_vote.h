#ifndef REQUEST_VOTE_H
#define REQUEST_VOTE_H

#include <types.h>

/* Stores the arguments being passed to the RequestVote threads */
typedef struct {
	int sockfd;
	RPCMsg msg;
} RequestVoteArgs;

void *RequestVoteThread(void *ptr);
RPCReplyMsg *RequestVote(int sockfd, uint16_t rpcType, uint32_t term, uint32_t candidateId, uint32_t lastLogIndex, uint32_t lastLogTerm);

#endif
