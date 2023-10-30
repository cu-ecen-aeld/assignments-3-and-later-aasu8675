/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
		size_t char_offset, size_t *entry_offset_byte_rtn )
{
	/**
	 * TODO: implement per description
	 */
	size_t bytes_read = 0;
	uint8_t index = buffer->out_offs;

	if(buffer == NULL)
		return NULL;

	while(bytes_read <= char_offset)
	{
		// Check if entry is valid
		if(buffer->entry[index].buffptr != NULL)
		{
			size_t entry_size = buffer->entry[index].size;

			// Check if updated total bytes read is greater than concatenated length of the entries
			if(bytes_read + entry_size > char_offset)
			{
				*entry_offset_byte_rtn = char_offset - bytes_read;
				return &buffer->entry[index];
			}
			bytes_read += entry_size;
		}
		index = (index+1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

		if(index == buffer->in_offs)
			break;
	}

	// Return null when position is out of range
	return NULL;
}

/**
 * Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
 * If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
 * new start location.
 * Any necessary locking must be handled by the caller
 * Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
 * Return value: Return the allocated buffer which needs to be freed in case of potential overwrite
 */
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
	/**
	 * TODO: implement per description
	 */

	// Copy the new entry into circular buffer
	buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    	buffer->entry[buffer->in_offs].size = add_entry->size;

	// Increment the in_offs for new entry and perform % for overflow check
        buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

	// If buffer is full, update the out_offset to overwrite the oldest entry
        if (buffer->full)
                buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

	// If the offsets are equal, update the buffer full status 
	if (buffer->in_offs == buffer->out_offs) 
		buffer->full = true;

}

/**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
	memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
