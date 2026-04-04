#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <types.h>
#include <arpa/inet.h>
#include <helper.h>

#include <append_entries.h>

/* Send Append Entries RPC call to server with sockfd, and receive a response
 * then return the results of the RPC call
 * returns NULL on any fail
 */
AppendResult *AppendEntries(int sockfd, int term, int leaderId, int prevLogIndex, int prevLogTerm,
	LogEntry *entries, int numEntries, int leaderCommit) {
	int check;

	RPCMsg msg;
	msg.rpcType = htons(APPEND);
	msg.term = htonl(term);
	msg.id = htonl(leaderId);
	msg.logIndex = htonl(prevLogIndex);
	msg.logTerm = htonl(prevLogTerm);
	msg.entriesLen = htonl(numEntries);
	msg.leaderCommit = htonl(leaderCommit);

	/* send RPC message */
	check = send(sockfd, &msg, sizeof(msg), 0);
	if(check <= 0) {
		printf("Error: did not send append entries header\n");
		perror("send");
		return NULL;
	}
	if(numEntries > 0) {
		int totalBytesToSend = numEntries * sizeof(LogEntry);
		check = sendMsgEntries(sockfd, entries, totalBytesToSend);
		if(check == -1) {
			return NULL;
		}
	}

	/* rec RPC reply message */
	RPCReplyMsg replyMsg;
	check = recv(sockfd, &replyMsg, sizeof(replyMsg), 0);
	if(check <= 0) {
		printf("Error: did not rec reply message for append rpc\n");
		perror("recv");
		return NULL;
	}
	if(ntohs(replyMsg.rpcType) != APPEND) {
		printf("Error: rec wrong rpc reply msg for append entries\n");
		return NULL;
	}

	/* return append result */
	AppendResult *result = (AppendResult *)malloc(sizeof(AppendResult));
	if(result == NULL) {
		perror("malloc");
		return NULL;
	}
	result->success = ntohl(replyMsg.result);
	result->term = ntohl(replyMsg.term);
	return result;
}
