#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "request_vote.h"

/* Thread wrapper function to call RequestVote */
void *RequestVoteThread(void *ptr) {
	RequestVoteArgs *args = (RequestVoteArgs *) ptr;

	RPCReplyMsg *result = RequestVote(args->sockfd, args->msg.rpcType, args->msg.term, args->msg.id, args->msg.logIndex, args->msg.logTerm);

	pthread_exit(result);
}

/* RPC Call */
RPCReplyMsg *RequestVote(int sockfd, uint16_t rpcType, uint32_t term, uint32_t id, uint32_t logIndex, uint32_t logTerm) {
	RPCMsg msg = {htons(rpcType), htons(term), htons(id), htons(logIndex), htons(logTerm)};
	RPCReplyMsg *reply = malloc(sizeof(RPCReplyMsg));

	if(send(sockfd, &msg, sizeof(msg), 0) == -1) {
		perror("RequestVote - send");
		exit(1);
	}

	printf("RequestVote send to fd %d\n", sockfd);

	if((recv(sockfd, reply, sizeof(RPCReplyMsg), 0)) == -1) {
		perror("RequestResult - recv");
		exit(2);
	}

	reply->term = htons(reply->term);
	reply->result = htons(reply->result);

	return reply;
}
