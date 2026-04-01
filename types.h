#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#define NUM_SERVERS 5 /* must be consistent with bash script */
#define HOST_LEN 64 /* string length for host name */

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
	DEL
} CommandType;

/* RPC types, for RPCMsg */
typedef enum {
	APPEND,
	VOTE
} RPCType;

/* a command, to be stored in log / committed to state */
typedef struct {
	CommandType type; /* type of command */
	char *x; /* var name */
	void *y; /* var val, for put */
} Command;

/* a log entry, containing a command and its term */
typedef struct {
	Command *cmd; /* command for log entry */
	int term; /* term that log entry was added */
} LogEntry;

/* a log entry to be sent over socket connection */
typedef struct {
	uint16_t cmdType; /* type of command */
	uint32_t xLen; /* length of var name */
	uint32_t yLen; /* length of val */
	uint32_t term; /* term */
} WireLogEntry;

/* a state machine entry, a key value pair */
typedef struct {
	char *key;
	void *val;
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

/* RPC message, used for both AppendEntries RPC and RequestVote RPC
 * described in format: <append> / <vote> -> <append functionality> / <vote functionality>
 * 	when receiving message, based on rpcType, will process vars differently based on description
 * need to read 
 */
typedef struct {
	uint16_t rpcType; /* Append / Vote -> converted RPCType */
	uint32_t term; /* leader / candidate -> term */
	uint32_t id; /* leader / candidate -> follower redirect clients / requesting vote */
	uint32_t logIndex; /* prev / last -> index immediately preceding new ones / last log entry index */
	uint32_t logTerm; /* prev / last -> prevLogIndex term / term of last log entry */

	/* Append only vars (set to default vals for Vote): */
	uint32_t entriesLen; /* len of entries -> entries sent separately as a stream of bytes after */
	uint32_t leaderCommit; /* commitIndex */
} RPCMsg;

#endif
