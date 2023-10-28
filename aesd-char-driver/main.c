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
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Aamir Suhail Burhan"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	PDEBUG("open");
	/**
	 * TODO: handle open
	 */
	aesd_device = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = &aesd_device;
	PDEBUG("open end");
	
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
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle read
	 */

	// Lock aesd device
	mutex_lock(&aesd_device.lock);

	struct aesd_buffer_entry = *entry;
	size_t entry_offset_byte = 0;

	entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buffer, *f_pos, &entry_offset_byte);

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
	mutex_unlock(&aesd_device.lock);

	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */

	// Lock the mutex
	mutex_lock(&aesd_device.lock);

	if(count > 0)
	{
		char *write_data = kmalloc(count, GFP_KERNEL);

		// Check for kmalloc failure
		if(!write_data)
		{
			PDEBUG("Kmalloc failed while writing");
			retval = -ENOMEM;
		}
		else 
		{
			// Copy from user space
			// Check for incomplete copies
			if(copy_from_user(write_data, buf, count))
				retval = -EFAULT;
			else
			{
				// Complete write
				struct aesd_buffer_entry new_entry;
				new_entry.buffptr = write_data;
				new_entry.size = count;
				aesd_circular_buffer_add_entry(&aesd_device.buffer, &new_entry);
				
				retval = count;
			}
		}
	}

	// Unlock the device 
	mutex_unlock(&aesd_device.lock);
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

	uint8_t i;
	struct aesd_buffer_entry *entry;

	// Freeing memory
	AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, i)
	{
		kfree(entry->buffptr);
	}

	// Destroy the mutex
	mutex_destroy(&aesd_device.lock);
	
	unregister_chrdev_region(devno, 1);
	PDEBUG("Cleanup End");
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
