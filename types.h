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

#endif
