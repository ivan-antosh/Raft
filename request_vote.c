#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "request_vote.h"

/* Thread wrapper function to call RequestVote */
void *RequestVoteThread(void *ptr) {
	RequestVoteArgs *args = (RequestVoteArgs *) ptr;

	RequestVote(args->sockfd, args->msg.rpcType, args->msg.term, args->msg.id, args->msg.logIndex, args->msg.logTerm);

	return NULL;
}

/* RPC Call - used to request leadership vote from other servers */
void RequestVote(int sockfd, uint16_t rpcType, uint32_t term, uint32_t id, uint32_t logIndex, uint32_t logTerm) {
	RPCMsg msg = {htons(rpcType), htonl(term), htonl(id), htonl(logIndex), htonl(logTerm), htonl(0), htonl(0)};

	if(send(sockfd, &msg, sizeof(msg), 0) == -1) {
		perror("RequestVote - send");
		exit(1);
	}

	printf("RequestVote send to fd %d\n", sockfd);
}
