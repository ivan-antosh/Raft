#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "request_vote.h"

void *RequestVoteThread(void *ptr) {
	RequestVoteArgs *args = (RequestVoteArgs *) ptr;

	RequestResult *result = RequestVote(args->sockfd, args->msg.rpcType, args->msg.term, args->msg.id, args->msg.logIndex, args->msg.logTerm);

	pthread_exit(result);
}

//RequestResult *RequestVote(int sockfd, int term, int id, int logIndex, int logTerm) {
RequestResult *RequestVote(int sockfd, int rpcType, int term, int id, int logIndex, int logTerm) {
	RPCMsg msg = {htons(rpcType), htons(term), htons(id), htons(logIndex), htons(logTerm)};
	RequestResult *reply = malloc(sizeof(RequestResult));

	if(send(sockfd, &msg, sizeof(msg), 0) == -1) {
		perror("RequestVote - send");
		exit(1);
	}

	printf("RequestVote send to fd %d\n", sockfd);

	if((recv(sockfd, reply, sizeof(RequestResult), 0)) == -1) {
		perror("RequestResult - recv");
		exit(2);
	}

	return reply;
}
