#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "request_vote.h"

/* Thread wrapper function to call RequestVote */
void *RequestVoteThread(void *ptr) {
	RequestVoteArgs *args = (RequestVoteArgs *) ptr;

	if(RequestVote(args->sockfd, args->msg.term, args->msg.candidateId, args->msg.lastLogIndex, args->msg.lastLogTerm) == -1) {
		printf("Error: request vote\n");
	}

	return NULL;
}

/* RPC Call - used to request leadership vote from other servers */
int RequestVote(int sockfd, uint32_t term, uint32_t candidateId, uint32_t lastLogIndex, uint32_t lastLogTerm) {
	RPCHeader header;
	header.rpcMsgType = htons(MSG);
	header.rpcType = htons(VOTE);
	/* send header */
	if(send(sockfd, &header, sizeof(header), 0) == -1) {
		perror("RequestVote header - send");
		return -1;
	}

	RPCVoteMsg msg;
	msg.term = htonl(term);
	msg.candidateId = htonl(candidateId);
	msg.lastLogIndex = htonl(lastLogIndex);
	msg.lastLogTerm = htonl(lastLogTerm);
	/* send message */
	if(send(sockfd, &msg, sizeof(msg), 0) == -1) {
		perror("RequestVote msg - send");
		return -1;
	}

	printf("RequestVote send to fd %d\n", sockfd);
	return 0;
}
