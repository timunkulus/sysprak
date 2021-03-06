#ifndef performConnection_h
#define performConnection_h

#define _POSIX_SOURCE 3

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include "config.h"
#include "string_helper.h"
#include "msg_creator.h"
#include "bashni_structs.h"
#include "court_helper.h"

//Enum for States of Client
typedef enum { PROLOG, COURSE, DRAFT }phase;

extern int fd_pipe_thinker;

//phase table function
typedef phase phase_func_t( phase_data *data );

void process_line(char* server_reply, int fd);

/**
 * Handles messages of Gameserver in Prolog
 */
phase handle_prolog(phase_data *data );

/**
 * Handles messages of Gameserver in Spielverlauf
 */
phase handle_course(phase_data *data );

/**
 * Handles messages of Gameserver in Spielzu
 */
phase handle_draft(phase_data *data );

/**
 * Handles messages of Gameserver (determin Method for actual state)
 */
phase run_phase( phase cur_phase, phase_data *data );

void handle_negativs(char* reply);

void performConnection(int fd,  int _shm_id);

void disconnect(int fd);

int send_to_gameserver(int fd, char *message);

#endif /* performConnection_h */
