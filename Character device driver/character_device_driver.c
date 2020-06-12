#include<linux/fs.h> //struct file, struct file_operations
#include<linux/init.h>//for __init, see code
#include<linux/module.h>//for __init, see code
#include<linux/miscdevice.h>//for module init and exit macros
#include<linux/uaccess.h>//for misc_device_register and struct miscdev
#include<asm/io.h>//for copy_to_user, see code
#include "../address_map_arm.h"
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include "../interrupt_ID.h"
//-------------------------------------------------------------------------------------------
/* Kernel character device driver /dev/LED. */
static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);
static ssize_t device_readKEY(struct file *, char *, size_t, loff_t *);
static ssize_t device_readSW(struct file *, char *, size_t, loff_t *);
static ssize_t device_writeLED(struct file *, const char *, size_t, loff_t *);
static ssize_t device_writeHEX(struct file *, const char *, size_t, loff_t *);
//-------------------------------------------------------------------------------------------
//FOPS for the different devices
static struct file_operations KEY_fops = {
		.owner = THIS_MODULE,
		.read = device_readKEY,
		//.write = device_write1,
		.open = device_open,
		.release = device_release
		};

static struct file_operations SW_fops = {
		.owner = THIS_MODULE,
		.read = device_readSW,
		//.write = device_write1,
		.open = device_open,
		.release = device_release
		};

static struct file_operations LED_fops = {
		.owner = THIS_MODULE,
		//.read = device_read1,
		.write = device_writeLED,
		.open = device_open,
		.release = device_release
		};

static struct file_operations HEX_fops = {
		.owner = THIS_MODULE,
		//.read = device_read1,
		.write = device_writeHEX,
		.open = device_open,
		.release = device_release
		};
//-------------------------------------------------------------------------------------------
//Macros
#define SUCCESS 0
#define DEV1_NAME "KEY"
#define DEV2_NAME "SW"
#define DEV3_NAME "LED"
#define DEV4_NAME "HEX"
#define MASK_Hex1 0xFF
#define MASK_Hex2 0xFFFF
#define MASK_Hex3 0xFFFFFF
//-------------------------------------------------------------------------------------------
//Assigning Device chara
static struct miscdevice KEY = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEV1_NAME,
		.fops = &KEY_fops,
		.mode = 0666
		};

static struct miscdevice SW = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEV2_NAME,
		.fops = &SW_fops,
		.mode = 0666
		};

static struct miscdevice LED = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEV3_NAME,
		.fops = &LED_fops,
		.mode = 0666
		};

static struct miscdevice HEX = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEV4_NAME,
		.fops = &HEX_fops,
		.mode = 0666
		};

//-------------------------------------------------------------------------------------------
static int KEY_registered = 0;
static int SW_registered = 0;
static int LED_registered = 0;
static int HEX_registered = 0;
#define MAX_SIZE 256
static char KEY_msg[MAX_SIZE];
static char SW_msg[MAX_SIZE];

// assume that no message longer than this will be used
static char LED_value[MAX_SIZE-1]; // the string that can be read or written
static char HEX_value[MAX_SIZE-1];
//-------------------------------------------------------------------------------------------
//Initializing pointers for the devices
volatile int *LED_ptr,*HEX_ptr1,*HEX_ptr2,*KEY_ptr,*SW_ptr;
void *LW_Virtual;
int KEY_capture,SW_capture,LED_capture,HEX_capture;
unsigned char arr[10]={0x3F,0X6,0X5B,0X4F,0X66,0X6D,0X7D,0X7,0X7F,0X67};//0-9 for 7-segment display
int value;

//-------------------------------------------------------------------------------------------
//Interrupt handler

// irq_handler_t irq_handler(int irq, void *dev_id, struct pt_regs *regs)
// 			{
// 			//*(KEY_ptr + 2) = 0x0;// Enabling interrupts for the key
// 			SW_capture=*(SW_ptr);
// 			KEY_capture=*(KEY_ptr+3);//capture the edgecapture reg state
// 			printk (KERN_ERR "interrupt");
// 			// Clear the Edgecapture register (clears current interrupt)
// 			*(KEY_ptr + 3) = 0xF;//clearing the edge capture
// 			//*(KEY_ptr + 2) = 0xF;// Enabling interrupts for the key

// 			return (irq_handler_t) IRQ_HANDLED;
// 			}


//-------------------------------------------------------------------------------------------
//Initializing function
static int __init init_drivers(void) {

			//Configuring the virtual Memory space
			LW_Virtual= ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);	
			LED_ptr=LW_Virtual+LEDR_BASE;
			KEY_ptr=LW_Virtual+KEY_BASE;
			SW_ptr=LW_Virtual+SW_BASE; 
			HEX_ptr1=LW_Virtual+HEX3_HEX0_BASE;
			HEX_ptr2=LW_Virtual+HEX5_HEX4_BASE;


			//Looking for errors
			int err = misc_register (&LED);
			int err2 = misc_register (&HEX);
			int err3 = misc_register (&KEY);
			int err4 = misc_register (&SW);

			//have to write code for printing which device
			if (err < 0) {
			printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV1_NAME);
			return err;//returns if device registration fails
			}
			else {
			printk (KERN_INFO "/dev/%s driver registered\n", DEV1_NAME);
			LED_registered = 1;
			}
			if (err2 < 0) {
			printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV2_NAME);
			return err2;
			}
			else {
			printk (KERN_INFO "/dev/%s driver registered\n", DEV2_NAME);
			HEX_registered = 1;
			}
			if (err3 < 0) {
			printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV3_NAME);
			return err3;
			}
			else {
			printk (KERN_INFO "/dev/%s driver registered\n", DEV3_NAME);
			KEY_registered = 1;
			}
			if (err4 < 0) {
			printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV4_NAME);
			return err4;
			}
			else {
			printk (KERN_INFO "/dev/%s driver registered\n", DEV4_NAME);
			SW_registered = 1;
			}
			
			*(KEY_ptr + 3) = 0xF; // Clear the Edgecapture register
			//*(KEY_ptr + 2) = 0xF;// Enabling interrupts for the key

			// value = request_irq (KEYS_IRQ, (irq_handler_t) irq_handler, IRQF_SHARED,
			// "pushbutton_irq_handler", (void *) (irq_handler));

			return err;
			}

//-------------------------------------------------------------------------------------------
//Have to deregister interrupts, and the other devices using the if cond
static void __exit stop_LED(void) {
		if (LED_registered || HEX_registered || KEY_registered || SW_registered) {
		misc_deregister (&KEY);
		misc_deregister (&SW);
		misc_deregister (&LED);
		misc_deregister (&HEX);
		*LED_ptr=0;
		*HEX_ptr1=0;
		*HEX_ptr2=0;
		*(KEY_ptr + 3) = 0xF; // Clear the Edgecapture register
		//*(KEY_ptr + 2) = 0;// Disabling interrupts for the key
		iounmap (LW_Virtual);
		//free_irq (KEYS_IRQ, (void *) irq_handler);//deregistering IRQ handler
		printk (KERN_INFO "/dev/%s driver de-registered\n", DEV1_NAME);
		}
		}
//-------------------------------------------------------------------------------------------
/* Called when a process opens LED */
static int device_open(struct inode *inode, struct file *file)
		{
		return SUCCESS;
		}
/* Called when a process closes LED */
static int device_release(struct inode *inode, struct file *file)
		{
		return 0;
		}
/* Called when a process reads from LED. Provides character data from
* LED_msg. Returns, and sets *offset to, the number of bytes read. */

//-------------------------------------------------------------------------------------------
//READ KEY
static ssize_t device_readKEY(struct file *filp, char *buffer, size_t length, loff_t *offset)
		{
		KEY_capture=*(KEY_ptr+3);
		sprintf(KEY_msg,"%d",KEY_capture);
		size_t bytes;
		bytes = strlen (KEY_msg) - (*offset);
		// how many bytes not yet sent?
		bytes = bytes > length ? length : bytes;
		// too much to send at once?
		if (bytes)
		(void) copy_to_user (buffer, &KEY_msg[*offset], bytes);
		// keep track of number of bytes sent to the user
		*offset = bytes;
		KEY_capture=0;
		*(KEY_ptr + 3) = 0xF;
		return bytes;
		}
/* Called when a process writes to LED. Stores the data received into
* LED_msg, and returns the number of bytes stored. */

//-------------------------------------------------------------------------------------------
//READ SW
static ssize_t device_readSW(struct file *filp, char *buffer, size_t length, loff_t *offset)
		{
		SW_capture=*(SW_ptr);
		sprintf(SW_msg,"%d",SW_capture);
		size_t bytes;
		bytes = strlen (SW_msg) - (*offset);
		// how many bytes not yet sent?
		bytes = bytes > length ? length : bytes;
		// too much to send at once?
		if (bytes)
		(void) copy_to_user (buffer, &SW_msg[*offset], bytes);
		// keep track of number of bytes sent to the user
		*offset = bytes;
		//SW_capture=0;
		return bytes;
		}
/* Called when a process writes to LED. Stores the data received into
* LED_msg, and returns the number of bytes stored. */
//-------------------------------------------------------------------------------------------
/* Called when a process writes to LED. Stores the data received into
* LED_msg, and returns the number of bytes stored. */
static ssize_t device_writeLED(struct file *filp, const char *buffer, size_t length, loff_t *offset)
		{
		size_t bytes;
		bytes = length;
		if (bytes > MAX_SIZE - 1)
		// can copy all at once, or not?
		bytes = MAX_SIZE - 1;
		(void) copy_from_user (LED_value, buffer, bytes);
		LED_value[bytes] ='\0'; // NULL terminate
		sscanf(LED_value,"%d",&LED_capture);
		*LED_ptr=LED_capture;
		// Note: we do NOT update *offset; we keep the last MAX_SIZE or fewer bytes
		return bytes;
		}

//-------------------------------------------------------------------------------------------
/* Called when a process writes to HEX. Stores the data received into
* HEX_msg, and returns the number of bytes stored. */
static ssize_t device_writeHEX(struct file *filp, const char *buffer, size_t length, loff_t *offset)
		{
		size_t bytes;
		bytes = length;
		if (bytes > MAX_SIZE - 1)
		// can copy all at once, or not?unsigned char arr[10]={0x3F,0X6,0X5B,0X4F,0X66,0X6D,0X7D,0X7,0X7F,0X67};
		bytes = MAX_SIZE - 1;
		(void) copy_from_user (HEX_value, buffer, bytes);
		HEX_value[bytes] ='\0'; // NULL terminate

		sscanf(HEX_value,"%d",&HEX_capture);//converting the string to integer

		//Printing the value onto the 7 segment display
		*HEX_ptr1=arr[HEX_capture%10];
		HEX_capture=HEX_capture/10;
		*HEX_ptr1=(*HEX_ptr1 & MASK_Hex1)+(arr[HEX_capture%10]<<8);
		HEX_capture=HEX_capture/10;
		*HEX_ptr1=(*HEX_ptr1 & MASK_Hex2)+(arr[HEX_capture%10]<<16);
		HEX_capture=HEX_capture/10;
		*HEX_ptr1=(*HEX_ptr1 & MASK_Hex3)+(arr[HEX_capture%10]<<24);	
		HEX_capture=HEX_capture/10;
		*HEX_ptr2=arr[HEX_capture%10];
		HEX_capture=HEX_capture/10;
		*HEX_ptr2=(*HEX_ptr2 & MASK_Hex1)+(arr[HEX_capture%10]<<8);
		//*HEX_ptr1=*HEX_value;
		// Note: we do NOT update *offset; we keep the last MAX_SIZE or fewer bytes
		return bytes;
		}
//-------------------------------------------------------------------------------------------
MODULE_LICENSE("GPL");
module_init (init_drivers);
module_exit (stop_LED);