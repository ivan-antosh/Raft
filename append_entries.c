#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <types.h>
#include <arpa/inet.h>
#include <helper.h>
#include <errno.h>

#include <append_entries.h>

/* Send Append Entries RPC call to server with sockfd, and receive a response
 * then return the results of the RPC call
 * returns NULL on any fail
 */
int AppendEntries(int sockfd, int term, int leaderId, int prevLogIndex, int prevLogTerm,
	LogEntry *entries, int numEntries, int leaderCommit) {
	int check;

	RPCHeader header;
	header.rpcMsgType = htons(MSG);
	header.rpcType = htons(APPEND);

	RPCAppendMsg msg;
	msg.term = htonl(term);
	msg.leaderId = htonl(leaderId);
	msg.prevLogIndex = htonl(prevLogIndex);
	msg.prevLogTerm = htonl(prevLogTerm);
	msg.entriesLen = htonl(numEntries);
	msg.leaderCommit = htonl(leaderCommit);

	/* send RPC header */
	if(send(sockfd, &header, sizeof(header), 0) == -1) {
		if(errno == EPIPE) {
			return -2;
		}
		printf("Error: did not send append entries header\n");
		perror("send");
		return -1;
	}

	/* send RPC message */
	if(send(sockfd, &msg, sizeof(msg), 0) == -1) {
		if(errno == EPIPE) {
			return -2;
		}
		printf("Error: did not send append entries msg\n");
		perror("send");
		return -1;
	}
	if(numEntries > 0) {
		int totalBytesToSend = numEntries * sizeof(LogEntry);
		check = sendMsgEntries(sockfd, entries, totalBytesToSend);
		if(check < 0) {
			return check;
		}
	}
	return 0;
}
