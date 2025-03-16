/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Hardik Minocha"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;

    /* Associate the device structure with the file using container_of macro
     * This allows us to access device data in other operations
     */
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    // Initialize the unfinished entry to ensure it starts clean
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    
    // Only initialize if it's not already in use
    if (!dev->unfinished_entry.buffptr) {
        dev->unfinished_entry.buffptr = NULL;
        dev->unfinished_entry.size = 0;
    }
    
    mutex_unlock(&dev->lock);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t entry_offset = 0;
    
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    // Acquire mutex for thread safety
    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    // Find the entry and offset corresponding to f_pos
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset);
    
    if (entry) {
        // Calculate how many bytes we can read
        size_t bytes_to_read = entry->size - entry_offset;
        if (bytes_to_read > count)
            bytes_to_read = count;

        // Copy data to user space
        if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_read)) {
            retval = -EFAULT;
        } else {
            *f_pos += bytes_to_read;
            retval = bytes_to_read;
        }
    }

    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev = filp->private_data;
    char *kernel_buf;
    bool has_newline = false;
    
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    // Allocate temporary kernel buffer
    kernel_buf = kmalloc(count, GFP_KERNEL);
    if (!kernel_buf) {
        return -ENOMEM;
    }

    // Copy data from user space
    if (copy_from_user(kernel_buf, buf, count)) {
        kfree(kernel_buf);
        return -EFAULT;
    }

    // Check for newline in this write
    has_newline = memchr(kernel_buf, '\n', count) != NULL;

    // Acquire mutex for thread safety
    if (mutex_lock_interruptible(&dev->lock)) {
        kfree(kernel_buf);
        return -ERESTARTSYS;
    }

    // Check if we need to append to existing entry or create new one
    if (dev->unfinished_entry.buffptr) {
        // Reallocate to accommodate new data
        char *new_buf = krealloc(dev->unfinished_entry.buffptr, 
                               dev->unfinished_entry.size + count, 
                               GFP_KERNEL);
        if (!new_buf) {
            mutex_unlock(&dev->lock);
            kfree(kernel_buf);
            return -ENOMEM;
        }
        dev->unfinished_entry.buffptr = new_buf;
        memcpy((char *)dev->unfinished_entry.buffptr + dev->unfinished_entry.size,
               kernel_buf, count);
        dev->unfinished_entry.size += count;
        kfree(kernel_buf);
    } else {
        dev->unfinished_entry.buffptr = kernel_buf;
        dev->unfinished_entry.size = count;
    }

    if (has_newline) {
        // Save the entry that might be overwritten if buffer is full
        if (dev->buffer.full) {
            char *buffptr_to_free = (char *)dev->buffer.entry[dev->buffer.out_offs].buffptr;
            aesd_circular_buffer_add_entry(&dev->buffer, &dev->unfinished_entry);
            if (buffptr_to_free) {
                kfree(buffptr_to_free);
            }
        } else {
            // Buffer not full, just add the entry
            aesd_circular_buffer_add_entry(&dev->buffer, &dev->unfinished_entry);
        }
        
        // Reset unfinished entry
        dev->unfinished_entry.buffptr = NULL;
        dev->unfinished_entry.size = 0;
    }

    retval = count;
    mutex_unlock(&dev->lock);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    // Initialize the mutex for thread safety
    mutex_init(&aesd_device.lock);
    // Initialize the circular buffer
    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    // Free any entries remaining in the circular buffer
    struct aesd_buffer_entry *entry;
    uint8_t index;
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
        if (entry->buffptr) {
            kfree(entry->buffptr);
        }
    }
    // Destroy the mutex
    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
