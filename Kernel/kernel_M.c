/*
@file kernel_M.c
@brief Kernel modul
@author Alan Muhemad
@date 2025-02-08

@details
creates character device for user space communication
validates the package (CRC Checksum)
timeslot management
writes data to the GPIO
*/
#include <linux/init.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/fs.h>         // For file_operations
#include <linux/uaccess.h>    // For copy_from_user
#include <linux/cdev.h>       // For cdev
#include <linux/device.h>     // For device_create, class_create
#include <linux/slab.h>       // For kmalloc, kfree

#include <linux/crc32.h>      // Include CRC32

#include <linux/gpio.h>      // GPIO support
#include <linux/delay.h>     // Delay functions
#include <linux/jiffies.h>   // Time measurement

#include <linux/kthread.h>   // Kernel threads
#include <linux/sched.h>     // Task scheduling

#define DEVICE_NAME "packet_receiver"
#define CLASS_NAME  "packet_class"

// cat /sys/kernel/debug/gpio
#define GPIO_DATA 17  // GPIO for data transmission
#define GPIO_CLOCK 27 // GPIO for clock control

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Me :)");
MODULE_DESCRIPTION("A kernel module to receive packets from userspace.");
MODULE_VERSION("1.7");

// The size of the receive buffer
#define RECV_BUF_SIZE 512

// Global variables for device number, class, device.
static dev_t dev_number;
static struct class *packet_class = NULL;
static struct cdev packet_cdev;

// Store last valid data packet
static char last_valid_packet[256] = {0}; 

// Time of last transmission
static struct task_struct *transmit_thread;

// Flag to control loop in the thread
static int keep_sending = 0;  

// This buffer will temporarily store data from userspace
static char *recv_buffer;

// Forward declarations for file operations
static int     packet_open(struct inode *inode, struct file *file);   //not used
static int     packet_release(struct inode *inode, struct file *file);   //not used
static ssize_t packet_read(struct file *filp, char __user *buf, size_t len, loff_t *offset);   //not used
static ssize_t packet_write(struct file *filp, const char __user *buf, size_t len, loff_t *offset);
static unsigned long calculate_crc32(const char *data, size_t len);
static int transmit_loop(void *data); // Kernel thread function

// File operations structure
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = packet_open,     //not used
    .read    = packet_read,     //not used
    .write   = packet_write,
    .release = packet_release,  //not used
};

// CRC32 Calculation Function
// uses the same polynomial as zlib’s standard CRC32
static u32 calculate_crc32(const char *data, size_t len) {
    return crc32(0, data, len);
}

// ====================== Device Open ======================
static int packet_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "packet_receiver: Device opened.\n");
    return 0;
}

// ====================== Kernel Thread for Continuous Sending ======================
static int transmit_loop(void *data) {
    while (!kthread_should_stop()) {
        if (keep_sending && strlen(last_valid_packet) > 0) {
            // Wait for clock signal
            if (gpio_get_value(GPIO_CLOCK) == 1) {
                printk(KERN_INFO "packet_receiver: Clock HIGH -> Pausing transmission.\n");
                msleep(100); // Small sleep to avoid busy looping
                continue;
            }

            // Transmit the last valid packet
            for (size_t i = 0; i < strlen(last_valid_packet); i++) {
                gpio_set_value(GPIO_DATA, 1);
                msleep(1);
                gpio_set_value(GPIO_DATA, 0);
                msleep(1);
            }

            printk(KERN_INFO "packet_receiver: Sent data to GPIO %d\n", GPIO_DATA);

            // Wait 3x the transmission time before sending again
            msleep(strlen(last_valid_packet) * 3);
        } else {
            msleep(100); // Wait if no valid packet
        }
    }
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

    unsigned int sender_hex = 0;
    int value_id_0 = -1, value_id_1 = -1;
    char value_str_0[64] = {0};
    char value_str_1[64] = {0};
    unsigned long crc_val = 0;

    printk(KERN_INFO "packet_receiver: Parsed fields [matches=%d]:\n", matches);
    printk(KERN_INFO "  sender=0x%X\n", sender_hex);
    printk(KERN_INFO "  value_id_0=%d  value_0=\"%s\"\n", value_id_0, value_str_0);
    printk(KERN_INFO "  value_id_1=%d  value_1=\"%s\"\n", value_id_1, value_str_1);
    printk(KERN_INFO "  crc=0x%lX\n", crc_val);

    // Prepare the payload for CRC verification
    char crc_buffer[256];
    snprintf(crc_buffer, sizeof(crc_buffer), "SENDER=0x%x VALUE_ID=%d VALUE=%s VALUE_ID=%d VALUE=%s",
        sender_hex, value_id_0, value_str_0, value_id_1, value_str_1);
        
    // Calculate CRC32 for the received data excluding the CRC field
    unsigned long computed_crc = calculate_crc32(crc_buffer, strlen(crc_buffer));
    printk(KERN_INFO "  computed_crc=0x%lX\n", computed_crc);

    // Check CRC32 before transmission
    if (computed_crc != crc_val) {
        printk(KERN_ERR "packet_receiver: CRC32 mismatch! Data rejected.\n");
        return -EIO;
    }

    // Store the valid packet
    strcpy(last_valid_packet, crc_buffer);
    keep_sending = 1;

    printk(KERN_INFO "packet_receiver: Sent data to GPIO %d\n", GPIO_DATA);

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

    gpio_request(GPIO_DATA, "packet_data");
    gpio_request(GPIO_CLOCK, "packet_clock");
    gpio_direction_output(GPIO_DATA, 0);
    gpio_direction_input(GPIO_CLOCK); // Clock is an input signal

    // Start the transmission thread
    transmit_thread = kthread_run(transmit_loop, NULL, "packet_transmitter");
    if (IS_ERR(transmit_thread)) {
        printk(KERN_ERR "packet_receiver: Failed to create transmit_thread.\n");
        kfree(recv_buffer);
        device_destroy(packet_class, dev_number);
        class_destroy(packet_class);
        cdev_del(&packet_cdev);
        unregister_chrdev_region(dev_number, 1);
        return PTR_ERR(transmit_thread);
    }

    printk(KERN_INFO "packet_receiver: Module loaded.\n");
    return 0;
}

// ====================== Module Exit ======================
static void __exit packet_exit(void) {

    // Stop the transmission thread
    kthread_stop(transmit_thread);

    // Free recv_buffer
    kfree(recv_buffer);

    // Remove device node
    device_destroy(packet_class, dev_number);
    // Destroy class
    class_destroy(packet_class);
    // Delete cdev
    cdev_del(&packet_cdev);
    // Free device numbers
    unregister_chrdev_region(dev_number, 1);
    // Free GPIOs
    gpio_free(GPIO_DATA);
    gpio_free(GPIO_CLOCK);

    printk(KERN_INFO "packet_receiver: Module unloaded.\n");
}

module_init(packet_init);
module_exit(packet_exit);
