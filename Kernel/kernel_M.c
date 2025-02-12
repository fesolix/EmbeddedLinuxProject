#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>         // For file_operations
#include <linux/uaccess.h>    // For copy_from_user
#include <linux/cdev.h>       // For cdev
#include <linux/device.h>     // For device_create, class_create
#include <linux/slab.h>       // For kmalloc, kfree
#include <linux/crc32.h>      // Include CRC32

#define DEVICE_NAME "packet_receiver"
#define CLASS_NAME  "packet_class"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Me :)");
MODULE_DESCRIPTION("A kernel module to receive packets from userspace.");
MODULE_VERSION("1.0");
/*
// Structure to hold our packet data
struct packet_data {
    int sender_id;
    int value_id_1;
    int value_1;
    int value_id_2;
    int value_2;
};
*/

// The size of the receive buffer
#define RECV_BUF_SIZE 512

// Global variables for device number, class, device.
static dev_t dev_number;
static struct class *packet_class = NULL;
static struct cdev packet_cdev;

// This buffer will temporarily store data from userspace
static char *recv_buffer;

// Forward declarations for file operations
static int     packet_open(struct inode *inode, struct file *file);   //not used
static int     packet_release(struct inode *inode, struct file *file);   //not used
static ssize_t packet_read(struct file *filp, char __user *buf, size_t len, loff_t *offset);   //not used
static ssize_t packet_write(struct file *filp, const char __user *buf, size_t len, loff_t *offset);

// File operations structure
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = packet_open,     //not used
    .read    = packet_read,     //not used
    .write   = packet_write,
    .release = packet_release,  //not used
};

// CRC32 Calculation Function
static u32 calculate_crc32(const char *data, size_t len) {
    return crc32(0, data, len);
}

// ====================== Device Open ======================
static int packet_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "packet_receiver: Device opened.\n");
    return 0;
}

// ====================== Device Read ======================
static ssize_t packet_read(struct file *filp, char __user *buf, size_t len, loff_t *offset) {
    printk(KERN_INFO "packet_receiver: Read called.\n");
    return 0;
}

// ====================== Device Write ======================
static ssize_t packet_write(struct file *filp, const char __user *buf, size_t len, loff_t *offset) {
    // Bound the incoming size to the size of the buffer
    size_t to_copy = min(len, (size_t)RECV_BUF_SIZE - 1);

    // Clear the buffer before copying data into it (for safety)
    memset(recv_buffer, 0, RECV_BUF_SIZE);

    // Copy data from user space into recv_buffer 
    if (copy_from_user(recv_buffer, buf, to_copy) != 0) {
        printk(KERN_ERR "packet_receiver: copy_from_user failed.\n");
        return -EFAULT;
    }

    // Ensure null-termination 
    recv_buffer[to_copy] = '\0';

    /* // Placeholders for the fields:
    struct packet_data p_data;
    p_data.sender_id  = -1;
    p_data.value_id_1 = -1;
    p_data.value_1    = -1;
    p_data.value_id_2 = -1;
    p_data.value_2    = -1;
*/
    unsigned int sender_hex = 0;
    int value_id_0 = -1, value_id_1 = -1;
    char value_str_0[64] = {0};
    char value_str_1[64] = {0};
    unsigned long crc_val = 0;
    
    // Example format:
    //   "SENDER=1 ID1=100 VAL1=222 ID2=101 VAL2=333"
    //   "SENDER=10 VID1=11 VAL1=100 VID2=12 VAL2=200"
/*
    sscanf(recv_buffer, "SENDER=%d VID1=%d VAL1=%d VID2=%d VAL2=%d",
           &p_data.sender_id,
           &p_data.value_id_1,
           &p_data.value_1,
           &p_data.value_id_2,
           &p_data.value_2);
*/
    int matches = sscanf(recv_buffer,
      "SENDER=0x%x VALUE_ID=%d VALUE=%63s VALUE_ID=%d VALUE=%63s CRC=0x%lx",
       &sender_hex,
       &value_id_0, value_str_0,
       &value_id_1, value_str_1,
       &crc_val);
    // Print the received packet data
   /* printk(KERN_INFO "packet_receiver: Received packet -> "
           "sender=0x%x, valID1=%d, val1=%d, valID2=%d, val2=%d\n",
           p_data.sender_id,
           p_data.value_id_1, p_data.value_1,
           p_data.value_id_2, p_data.value_2);
*/
    printk(KERN_INFO "packet_receiver: Parsed fields [matches=%d]:\n", matches);
    printk(KERN_INFO "  sender=0x%X\n", sender_hex);
    printk(KERN_INFO "  value_id_0=%d  value_0=\"%s\"\n", value_id_0, value_str_0);
    printk(KERN_INFO "  value_id_1=%d  value_1=\"%s\"\n", value_id_1, value_str_1);
    printk(KERN_INFO "  crc=0x%lX\n", crc_val);

    // Calculate CRC32 for the received data excluding the CRC field
    size_t crc_len = strrchr(recv_buffer, 'C') - recv_buffer;
    u32 calculated_crc = calculate_crc32(recv_buffer, crc_len);

    if (calculated_crc == received_crc) {
        printk(KERN_INFO "packet_receiver: CRC check passed!\n");
    } else {
        printk(KERN_ERR "packet_receiver: CRC mismatch! Expected: 0x%08X, Received: 0x%08lX\n",
               calculated_crc, received_crc);
    }

    // Return the number of bytes written
    return len;
}

// ====================== Device Release ======================
static int packet_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "packet_receiver: Device closed.\n");
    return 0;
}

// ====================== Module Init ======================
static int __init packet_init(void) {
    int ret;

    // Allocate a device number dynamically
    ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "packet_receiver: Failed to allocate major/minor.\n");
        return ret;
    }
    printk(KERN_INFO "packet_receiver: Registered with major=%d, minor=%d\n",
           MAJOR(dev_number), MINOR(dev_number));

    // Initialize the cdev structure 
    cdev_init(&packet_cdev, &fops);
    packet_cdev.owner = THIS_MODULE;

    // Add the cdev to the kernel 
    ret = cdev_add(&packet_cdev, dev_number, 1);
    if (ret < 0) {
        printk(KERN_ERR "packet_receiver: Unable to add cdev.\n");
        unregister_chrdev_region(dev_number, 1);
        return ret;
    }

    // Create a class for udev 
    packet_class = class_create(CLASS_NAME);
    if (IS_ERR(packet_class)) {
        printk(KERN_ERR "packet_receiver: Failed to create class.\n");
        cdev_del(&packet_cdev);
        unregister_chrdev_region(dev_number, 1);
        return PTR_ERR(packet_class);
    }

    // Create a device node /dev/packet_receiver
    if (IS_ERR(device_create(packet_class, NULL, dev_number, NULL, DEVICE_NAME))) {
        printk(KERN_ERR "packet_receiver: Failed to create device.\n");
        class_destroy(packet_class);
        cdev_del(&packet_cdev);
        unregister_chrdev_region(dev_number, 1);
        return -1;
    }

    // Allocate memory for receive buffer
    recv_buffer = kmalloc(RECV_BUF_SIZE, GFP_KERNEL);
    if (!recv_buffer) {
        printk(KERN_ERR "packet_receiver: Failed to allocate recv_buffer.\n");
        device_destroy(packet_class, dev_number);
        class_destroy(packet_class);
        cdev_del(&packet_cdev);
        unregister_chrdev_region(dev_number, 1);
        return -ENOMEM;
    }

    printk(KERN_INFO "packet_receiver: Module loaded.\n");
    return 0;
}

// ====================== Module Exit ======================
static void __exit packet_exit(void) {
    // Free recv_buffer
    if (recv_buffer) {
        kfree(recv_buffer);
    }

    // Remove device node
    device_destroy(packet_class, dev_number);
    // Destroy class
    class_destroy(packet_class);
    // Delete cdev
    cdev_del(&packet_cdev);
    // Free device numbers
    unregister_chrdev_region(dev_number, 1);

    printk(KERN_INFO "packet_receiver: Module unloaded.\n");
}

module_init(packet_init);
module_exit(packet_exit);
