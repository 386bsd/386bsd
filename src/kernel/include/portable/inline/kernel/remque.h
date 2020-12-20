/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: $
 * Function to remove an element from a queue.
 */
__INLINE void
_remque(queue_t element)
{
	(element->next)->prev = element->prev;
	(element->prev)->next = element->next;
	element->prev = (queue_t)0;
}
