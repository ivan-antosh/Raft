#ifndef TYPES_H
#define TYPES_H

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

#endif
