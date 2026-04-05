#ifndef REQUEST_VOTE_H
#define REQUEST_VOTE_H

#include <types.h>

/* Stores the arguments being passed to the RequestVote threads */
int RequestVote(int sockfd, int term, int candidateId, int lastLogIndex, int lastLogTerm);

#endif
