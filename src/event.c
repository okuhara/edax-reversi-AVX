/**
 * @file event.c
 *
 * Event management.
 *
 * @date 1998 - 2026
 * @author Richard Delorme
 * @version 4.5
 */

#include "event.h"
#include "util.h"

#include <stdlib.h>


/**
 * @brief Initialize a message event.
 *
 * @param event Event.
 */
void event_init(Event *event)
{
	int i;

	event->size = 4;
	event->first = 0;
	event->end = 0;
	event->ring = (char**) malloc(event->size * sizeof (char*));
	if (event->ring == NULL) fatal_error("cannot allocate event buffers\n");
	for (i = 0; i < event->size; ++i) event->ring[i] = NULL;
	spin_init(&event->spin);
	lock_init(&event->lock);
	condition_init(&event->cond);

	event->loop = true;
	event->reading = event->closing = false;
}

/**
 * @brief Remove all unprocessed messages.
 *
 * @param event Event.
 */
void event_clear_messages(Event *event)
{
	int i;

	spin_lock(&event->spin);

	for (i = 0; i < event->size; ++i) {
		free(event->ring[i]);
		event->ring[i] = NULL;
	}
	event->first = 0;
	event->end = 0;

	spin_unlock(&event->spin);
}


/**
 * @brief Free a message event.
 *
 * @param event Event.
 */
void event_free(Event *event)
{
	int i;

	spin_lock(&event->spin);

	for (i = 0; i < event->size; ++i) {
		free(event->ring[i]);
	}
	event->first = 0;
	event->end = 0;
	free(event->ring);

	lock_free(&event->lock);
	condition_free(&event->cond);

	spin_unlock(&event->spin);
	spin_free(&event->spin);
}

/**
 * @brief Add a new message at the bottom of the list.
 *
 * @param event Event.
 * @param message New message.
 */
void event_add_message(Event *event, char *message)
{
	char **new_ring;
	int new_size, i;
	int last;

	spin_lock(&event->spin);

	last = event->end;
	event->end = (event->end + 1) % event->size;

	if (event->end == event->first) {
		new_size = event->size * 2;
		new_ring = (char**) malloc(new_size * sizeof (char*));
		if (new_ring == NULL) fatal_error("cannot allocate event buffers\n");
		for (i = 0; i < event->size; ++i)
			new_ring[i] = event->ring[(i + event->first) % event->size];
		for (; i < new_size; ++i) new_ring[i] = NULL;
		event->first = 0;
		event->end = event->size;
		last = event->size - 1;
		event->size = new_size;
		free(event->ring);
		event->ring = new_ring;
	}

	event->ring[last] = message;
	info("[event add [%d]: %s]\n", last, message);

	spin_unlock(&event->spin);
}


/**
 * @brief Wait input.
 *
 * @param event Event.
 * @param cmd Command.
 * @param param Command's parameters.
 */
void event_wait(Event *event, char **cmd, char **param)
{
	int n;
	char *message;

	lock(&event->lock);
	while ((message = event_peek_message(event)) == NULL) {
		condition_wait(&event->cond, &event->lock);
	}
	unlock(&event->lock);

	free(*cmd);
	free(*param);

	info("[event wait: %s]\n", message);

	n = strlen(message);
	*cmd = (char*) malloc(n + 1);
	*param = (char*) malloc(n + 1);
	parse_command(message, *cmd, *param, n + 1);
	free(message);
}

void event_wait_enter(Event *event)
{
	char *message;
	puts("Press [Enter] to continue");
	lock(&event->lock);
	while ((message = event_peek_message(event)) == NULL) {
		condition_wait(&event->cond, &event->lock);
	}
	unlock(&event->lock);
	free(message);
}

/**
 * @brief Check if there is a message.
 *
 * This function is lockless, and should be used only when the event is locked.
 *
 * @param event Event;
 * @return true if a message exists.
 */
bool event_exist(Event *event)
{
	return (event->first != event->end);
}

/**
 * @brief Peek the first message from the list.
 *
 * @param event Event;
 * @return the message.
 */
char *event_peek_message(Event *event)
{
	char *message;

	spin_lock(&event->spin);

	if (event_exist(event)) {
		message = event->ring[event->first];
		event->ring[event->first] = NULL;
		event->first = (event->first + 1) % event->size;
	} else {
		message = NULL;
	}

	spin_unlock(&event->spin);

	return message;
}
