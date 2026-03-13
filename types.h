#ifndef TYPES_H
#define TYPES_H

#define NUM_SERVERS 5 /* must be consistent with bash script */

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

#endif
