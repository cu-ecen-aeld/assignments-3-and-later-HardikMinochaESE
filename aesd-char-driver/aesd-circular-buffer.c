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
    * Find entry containing the char_offset and store corresponding byte offset in entry_offset_byte_rtn
    */
    size_t cumulative_size = 0;
    uint8_t current_index = buffer->out_offs;
    int entries_checked = 0;
    
    // Handle empty buffer case
    if (!buffer->full && buffer->in_offs == buffer->out_offs) {
        return NULL;
    }
    
    // Calculate total bytes and find correct entry
    do {
        if (buffer->entry[current_index].buffptr) {
            if (char_offset < (cumulative_size + buffer->entry[current_index].size)) {
                *entry_offset_byte_rtn = char_offset - cumulative_size;
                return &buffer->entry[current_index];
            }
            cumulative_size += buffer->entry[current_index].size;
        }
        
        current_index = (current_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        entries_checked++;
    } while ((entries_checked < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) && 
             (current_index != buffer->in_offs || buffer->full));
    
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    // If buffer is full, we need to advance out_offs before adding new entry
    if (buffer->full) {
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        buffer->full = false;  // Buffer won't be full after advancing out_offs
    }
    
    // Check if this entry should be combined with the previous one
    if (buffer->in_offs > 0 || buffer->full) {
        uint8_t prev_index = (buffer->in_offs == 0) ? 
            AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1 : buffer->in_offs - 1;
            
        // If previous entry exists and doesn't end with newline, combine them
        if (buffer->entry[prev_index].buffptr && 
            buffer->entry[prev_index].size > 0 &&
            buffer->entry[prev_index].buffptr[buffer->entry[prev_index].size - 1] != '\n') {
            // This entry should be combined with the previous one
            buffer->in_offs = prev_index;  // Move back to modify previous entry
        }
    }
    
    // Copy the new entry to the current write position
    buffer->entry[buffer->in_offs] = *add_entry;
    
    // Only advance write position if this entry ends with newline
    if (add_entry->size > 0 && 
        add_entry->buffptr[add_entry->size - 1] == '\n') {
        buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        if (buffer->in_offs == buffer->out_offs) {
            buffer->full = true;
        }
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
