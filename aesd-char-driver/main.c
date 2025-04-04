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
#include <linux/uaccess.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"
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
        // Cast to void* to avoid const warning
        memcpy((void *)(dev->unfinished_entry.buffptr + dev->unfinished_entry.size),
               kernel_buf, count);
        dev->unfinished_entry.size += count;
        kfree(kernel_buf);
    } else {
        dev->unfinished_entry.buffptr = kernel_buf;
        dev->unfinished_entry.size = count;
    }

    // Check if we have a complete line
    char *newline_pos = memchr(dev->unfinished_entry.buffptr, 
                              '\n', 
                              dev->unfinished_entry.size);
    
    if (newline_pos) {
        // We have a complete line, add it to circular buffer
        // Save current entry in case it needs to be freed
        struct aesd_buffer_entry *entry_to_free = NULL;
        if (dev->buffer.full) {
            entry_to_free = &dev->buffer.entry[dev->buffer.out_offs];
        }
        
        // Add the entry to circular buffer
        aesd_circular_buffer_add_entry(&dev->buffer, &dev->unfinished_entry);
        
        // If buffer was full, free the overwritten entry
        if (entry_to_free && entry_to_free->buffptr) {
            kfree(entry_to_free->buffptr);
        }
        
        // Reset unfinished entry
        dev->unfinished_entry.buffptr = NULL;
        dev->unfinished_entry.size = 0;
    }

    retval = count;
    mutex_unlock(&dev->lock);
    return retval;
}

/**
 * Implement seek functionality
 */
loff_t aesd_llseek(struct file *filp, loff_t offset, int seek_type)
{
    struct aesd_dev *device = filp->private_data;
    loff_t new_position;
    size_t buffer_total_size = 0;
    uint8_t buffer_index;
    struct aesd_buffer_entry *buffer_entry;

    // Acquire mutex for thread safety
    if (mutex_lock_interruptible(&device->lock))
        return -ERESTARTSYS;

    // Calculate total size of all entries in circular buffer
    AESD_CIRCULAR_BUFFER_FOREACH(buffer_entry, &device->buffer, buffer_index) {
        if (buffer_entry->buffptr && buffer_entry->size > 0) {
            buffer_total_size += buffer_entry->size;
        }
    }

    // Handle different seek types
    switch(seek_type) {
    case SEEK_SET:
        new_position = offset;
        break;
    case SEEK_CUR:
        new_position = filp->f_pos + offset;
        break;
    case SEEK_END:
        new_position = buffer_total_size + offset;
        break;
    default:
        mutex_unlock(&device->lock);
        return -EINVAL;
    }

    // Validate the new position
    if (new_position < 0) {
        mutex_unlock(&device->lock);
        return -EINVAL;
    }

    // Update file position
    filp->f_pos = new_position;
    mutex_unlock(&device->lock);
    
    return new_position;
}

/**
 * Implementation of ioctl for seeking to specific command and offset
 */
long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_dev *device = filp->private_data;
    struct aesd_seekto seekto;
    loff_t target_position = 0;
    uint8_t cmd_index;
    uint32_t current_cmd = 0;
    struct aesd_buffer_entry *buffer_entry;
    
    // Verify command is valid
    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) return -ENOTTY;

    switch (cmd) {
    case AESDCHAR_IOCSEEKTO:
        // Copy seekto struct from user space
        if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto))) {
            return -EFAULT;
        }

        // Acquire mutex for thread safety
        if (mutex_lock_interruptible(&device->lock))
            return -ERESTARTSYS;

        // Find the target command and calculate position
        AESD_CIRCULAR_BUFFER_FOREACH(buffer_entry, &device->buffer, cmd_index) {
            if (!buffer_entry->buffptr || buffer_entry->size == 0)
                continue;

            if (current_cmd == seekto.write_cmd) {
                // Found the target command
                if (seekto.write_cmd_offset >= buffer_entry->size) {
                    // Offset is beyond command length
                    mutex_unlock(&device->lock);
                    return -EINVAL;
                }
                target_position += seekto.write_cmd_offset;
                filp->f_pos = target_position;
                mutex_unlock(&device->lock);
                return 0;
            }
            target_position += buffer_entry->size;
            current_cmd++;
        }

        // Command not found
        mutex_unlock(&device->lock);
        return -EINVAL;

    default:
        return -ENOTTY;
    }
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
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
