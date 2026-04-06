#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#include "request_vote.h"

/* RPC Call - used to request leadership vote from other servers */
int RequestVote(int sockfd, int term, int candidateId, int lastLogIndex, int lastLogTerm) {
	RPCHeader header;
	header.rpcMsgType = htons(MSG);
	header.rpcType = htons(VOTE);
	/* send header */
	if(send(sockfd, &header, sizeof(header), 0) == -1) {
		if(errno == EPIPE || errno == EBADF) {
			return -2;
		}
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
		if(errno == EPIPE) {
			return -2;
		}
		perror("RequestVote msg - send");
		return -1;
	}

	return 0;
}
