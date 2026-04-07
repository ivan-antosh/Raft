#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#define NUM_SERVERS 5 /* must be consistent with bash script */
#define HOST_LEN 64 /* string length for host name */
#define KEY_LEN 32 /* max len for key */

/* possible server states, follower, candidate, leader */
typedef enum {
	FOLLOWER,
	CANDIDATE,
	LEADER
} ServerStateType;

/* possible command types, put, get, delete */
typedef enum {
	PUT,
	GET,
	DEL,
	NOOP
} CommandType;

/* RPC types, for RPCHeaderMsg */
typedef enum {
	APPEND,
	VOTE
} RPCType;

/* RPC msg types, for RPCHeaderMsg */
typedef enum {
	MSG,
	REPLY
} RPCMsgType;

/* a command, to be stored in log / committed to state */
typedef struct {
	CommandType type; /* type of command */
	char x[KEY_LEN]; /* key */
	int y; /* val, for PUT */
} Command;

/* a log entry, containing a command and its term */
typedef struct {
	Command cmd; /* command for log entry */
	int term; /* term that log entry was added */
} LogEntry;

/* a state machine entry, a key value pair */
typedef struct {
	char key[KEY_LEN];
	int val;
} StateEntry;

/* Server info to store for each server, for connection/communication purposes */
typedef struct {
	int id; /* unique, in range 1 - NUM_SERVERS */
	char hostname[HOST_LEN];
	int portNum;
	int sockfd; /* init to -1 until connected */
} ServerInfo;

/* Handshake message to indicate connected to server on connection */
typedef struct {
	uint32_t id;
} HandshakeMsg;

/* RPC header msg to send before the body (msg or reply msg) */
typedef struct {
	uint16_t rpcMsgType; /* Msg / ReplyMsg -> converted RPCHeaderType */
	uint16_t rpcType; /* Append / Vote -> converted RPCType */
} RPCHeader;

/* RPC message for append entries */
typedef struct {
	uint32_t term; /* leaders term */
	uint32_t leaderId; /* so follower can redirect clients */
	uint32_t prevLogIndex; /* index immediately preceding new ones */
	uint32_t prevLogTerm; /* prevLogIndex term */
	uint32_t entriesLen; /* len of entries -> entries sent separately as a stream of bytes after */
	uint32_t leaderCommit; /* leaders commitIndex */
} RPCAppendMsg;

/* RPC message for request vote */
typedef struct {
	uint32_t term; /* candidates term */
	uint32_t candidateId; /* candidate requesting vote */
	uint32_t lastLogIndex; /* candidates last log entry index */
	uint32_t lastLogTerm; /* candidates term of last log entry */
} RPCVoteMsg;

/* RPC reply message for append entries */
typedef struct {
	uint32_t term; /* term to update leader */
	uint32_t success; /* if follower contained entry matching prevLogIndex and prevLogTerm */
} RPCAppendReplyMsg;

/* RPC reply message for request vote */
typedef struct {
	uint32_t term; /* term to update candidate */
	uint32_t voteGranted; /* whether candidate received vote */
} RPCVoteReplyMsg;

#endif
