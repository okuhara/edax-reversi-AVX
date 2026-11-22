/**
 * @file event.h
 *
 * Event management.
 *
 * @date 1998 - 2017
 * @author Richard Delorme
 * @version 4.4
 */

#ifndef EDAX_EVENT_H
#define EDAX_EVENT_H

#include <stdbool.h>
#include "util.h"

/** Event management data */
typedef struct Event {
	volatile bool loop;         /*!< flag allowing to loop, waiting for events */
	volatile bool reading;      /*!< the input thread of the user interface is running */
	volatile bool closing;      /*!< the user interface ends: its input thread must not use it any more */
	bool busy;                  /*!< a message taken is being executed (under the lock) */
	int stop_left;              /*!< messages that the last stop stops and that are not done: the one executed, and those that waited */
	char **ring;                /*!< ring of buffers */
	int size;                   /*!< size of the buffer ring */
	int first;                  /*!< first position in the ring */
	int end;                    /*!< one past the last position in the ring */
	Thread thread;              /*!< thread */
	SpinLock spin;              /*!< spin lock */
	Lock lock;                  /*!< lock */
	Condition cond;             /*!< condition */
} Event;                        /*!< messages */

void event_init(Event*);
void event_free(Event*);
void event_clear_messages(Event*);
int event_count_messages(Event*);
void event_add_message(Event*, char*);
char *event_peek_message(Event*);
bool event_exist(Event*);
void event_wait(Event*, char**, char**);
void event_wait_enter(Event*);

#endif /* EDAX_EVENT_H */

