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
#include <linux/slab.h>
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Aamir Suhail Burhan"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static char *global_write_buffer = NULL;
static size_t global_write_buffer_size = 0;

int aesd_open(struct inode *inode, struct file *filp)
{
	PDEBUG("open");
	/**
	 * TODO: handle open
	 */
	struct aesd_dev *dev= container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev;
	PDEBUG("open end");

	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
	/**
	 * TODO: handle release
	 */
	filp->private_data = NULL;
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	ssize_t retval = 0;
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

	if((filp == NULL) || (buf == NULL))
	{
		return -EINVAL;
	}

	/**
	 * TODO: handle read
	 */
	struct aesd_buffer_entry *entry;
	size_t entry_offset_byte = 0;
	struct aesd_dev *dev = filp->private_data;

	// Lock aesd device
	if(mutex_lock_interruptible(&dev->lock) != 0)
	{
		PDEBUG("Error in read mutex locking");
		return -ERESTARTSYS;
	}

	entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset_byte);

	if(entry)
	{
		size_t remaining_bytes = entry->size - entry_offset_byte;
		size_t bytes_to_copy = min(remaining_bytes, count);

		if (copy_to_user(buf, entry->buffptr + entry_offset_byte, bytes_to_copy)) 
		{
			retval = -EFAULT;
		} 
		else 
		{
			retval = bytes_to_copy;
			*f_pos += retval;
		}
	}

	// Unlock the mutex
	mutex_unlock(&dev->lock);

	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	
	if((filp == NULL) || (buf == NULL))
        {
                return -EINVAL;
        }

	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */

	struct aesd_dev *dev = filp->private_data;

	char *write_data = kmalloc(count, GFP_KERNEL);

	// Check for kmalloc failure
	if(!write_data)
	{
		PDEBUG("Kmalloc failed while writing");
		return retval;
	}

	if (copy_from_user(write_data, buf, count)) 
	{
		retval = -EFAULT;
		kfree(write_data);
		return retval;
	}

	// Lock the mutex
	if(mutex_lock_interruptible(&dev->lock) != 0)
        {
                PDEBUG("Error in write mutex locking");
                return -ERESTARTSYS;
        }

	size_t write_index = 0;  // Index for writing into the buffer
	bool newline_found = false;

	// Iterate through the write command and handle newline characters
	while (write_index < count) 
	{
		if (write_data[write_index] == '\n') 
		{
			newline_found = true;
			break;
		}
		write_index++;
	}

	size_t append_index = 0;

	if(newline_found == true)
		append_index = write_index + 1;
	else
		append_index = count;
	if(global_write_buffer_size == 0)
	{
		global_write_buffer = kmalloc(count, GFP_KERNEL);
		if(!global_write_buffer)
		{
			kfree(write_data);
			return retval;
		}
	}
	else
	{
		global_write_buffer = krealloc(global_write_buffer, (append_index + global_write_buffer_size), GFP_KERNEL);

		if(!global_write_buffer)
		{
			kfree(write_data);
			return retval;
		}
	}

	memcpy(global_write_buffer + global_write_buffer_size, write_data, append_index);
	global_write_buffer_size += (append_index);


	if (newline_found)
	{
		struct aesd_buffer_entry add_entry;

		add_entry.size = global_write_buffer_size;
		add_entry.buffptr = global_write_buffer;

		PDEBUG("New Buffer: %s", global_write_buffer);

		struct aesd_buffer_entry *oldest_entry = NULL;

		if (dev->buffer.full)
                {
                        oldest_entry = &dev->buffer.entry[dev->buffer.in_offs];		
      		        if(oldest_entry->buffptr)
				kfree(oldest_entry->buffptr);
                        oldest_entry->buffptr = NULL;
                        oldest_entry->size = 0;
                }


		aesd_circular_buffer_add_entry(&dev->buffer, &add_entry);

		global_write_buffer_size = 0;
	}

	retval = append_index;

	if(write_data)
		kfree(write_data);

	// Unlock the mutex
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
	aesd_circular_buffer_init(&aesd_device.buffer);  // Initializing buffer
	mutex_init(&aesd_device.lock);  // Mutex Initialization

	//global_write_buffer = kmalloc(1, GFP_KERNEL); // Dummy Initialization
	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	PDEBUG("Cleanup start");
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */

	uint8_t i = 0;
	struct aesd_buffer_entry *entry;

	// Freeing memory
	AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, i)
	{
		if(entry->buffptr)
		{
			kfree(entry->buffptr);
			entry->buffptr = NULL;
		}
	}

	// Clear global buffer it exists and reset its size
	if(global_write_buffer)
	{
		kfree(global_write_buffer);
		global_write_buffer = NULL;
		global_write_buffer_size = 0;
	}


	// Destroy the mutex
	mutex_destroy(&aesd_device.lock);

	unregister_chrdev_region(devno, 1);
	PDEBUG("Cleanup End");
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
