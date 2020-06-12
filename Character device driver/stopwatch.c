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

static ssize_t device_readTIMER(struct file *, char *, size_t, loff_t *);
static ssize_t device_readKEY(struct file *, char *, size_t, loff_t *);
static ssize_t device_readSW(struct file *, char *, size_t, loff_t *);

static ssize_t device_writeTIMER(struct file *, const char *, size_t, loff_t *);
static ssize_t device_writeLED(struct file *, const char *, size_t, loff_t *);

//-------------------------------------------------------------------------------------------
//FOPS for the different devices

static struct file_operations TIMER_fops = {
		.owner = THIS_MODULE,
		.read = device_readTIMER,
		.write = device_writeTIMER,
		.open = device_open,
		.release = device_release
		};

static struct file_operations LED_fops = {
		.owner = THIS_MODULE,
		.write = device_writeLED,
		.open = device_open,
		.release = device_release
		};

static struct file_operations KEY_fops = {
		.owner = THIS_MODULE,
		.read = device_readKEY,
		.open = device_open,
		.release = device_release
		};

static struct file_operations SW_fops = {
		.owner = THIS_MODULE,
		.read = device_readSW,
		.open = device_open,
		.release = device_release
		};
//-------------------------------------------------------------------------------------------
//Macros
#define SUCCESS 0
#define DEV1_NAME "LED"
#define DEV2_NAME "KEY"
#define DEV3_NAME "SW"
#define DEV4_NAME "TIMER"
#define MASK_Hex1 0xFF
#define MASK_Hex2 0xFFFF
#define MASK_Hex3 0xFFFFFF
//-------------------------------------------------------------------------------------------
//Assigning Device chara

static struct miscdevice LED = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEV1_NAME,
		.fops = &LED_fops,
		.mode = 0666
		};

static struct miscdevice KEY = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEV2_NAME,
		.fops = &KEY_fops,
		.mode = 0666
		};

static struct miscdevice SW = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEV3_NAME,
		.fops = &SW_fops,
		.mode = 0666
		};

static struct miscdevice TIMER = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = DEV4_NAME,
		.fops = &TIMER_fops,
		.mode = 0666
		};

//-------------------------------------------------------------------------------------------
static int LED_registered = 0;
static int KEY_registered = 0;
static int SW_registered = 0;
static int TIMER_registered = 0;

#define MAX_SIZE 256

static char CMD_value[MAX_SIZE];


// assume that no message longer than this will be used
static char LED_value[MAX_SIZE-1]; // the string that can be read or written
static char TIMER_msg[MAX_SIZE];
static char SW_msg[MAX_SIZE];
static char KEY_msg[MAX_SIZE];

//-------------------------------------------------------------------------------------------
//Initializing pointers for the devices

volatile int *LED_ptr,*HEX_ptr,*HEX_ptr1,*TIMER_ptr,*KEY_ptr,*SW_ptr;
void *LW_Virtual;

int HEX_capture;
int KEY_capture;
int SW_capture;
int LED_capture;
int Button;
unsigned char mag_num[10]={0x3F,0X6,0X5B,0X4F,0X66,0X6D,0X7D,0X7,0X7F,0X67};//0-9 for 7-segment display
int value;

static char end[MAX_SIZE]="end";
static char run[MAX_SIZE]="run";



int microsec=99;
int sec=59;
int minutes=59;
int idxToDel = 2;
int dsd=4;
int mmssdd=0;
int flag=0;
//-------------------------------------------------------------------------------------------
//Interrupt handler for timer

irq_handler_t irq_handler(int irq, void *dev_id, struct pt_regs *regs)
			{

			//timer 
			*(TIMER_ptr)=0;
			microsec=microsec-1;
			if(minutes>=59)
				minutes=59;
			if(sec>=59)
				sec=59;
			if(microsec>=99)
				microsec=99;
			if(minutes>0 && microsec<=00)
			{
				sec=sec-1;
				microsec =99;
			}
			if(minutes>0 && sec<=0)
			{
				minutes=minutes-1;
				sec=59;
				microsec=99;
			}
			
			if(minutes==0)
			{
				if(microsec<0 && sec>0)
				{	sec=sec-1;
					microsec=99;
				}
				if(microsec<0 && sec==0)
				{
					sec=0;
					microsec=0;
				}
			}
			if(minutes==00 && sec==0 && microsec==0)

			{
				minutes=00;
				sec=00;
				microsec=00;
				*(TIMER_ptr+1)=11;//stop timer if 00 s reached
			}
		
			
			*HEX_ptr=mag_num[microsec%10];
			*HEX_ptr=(*HEX_ptr & MASK_Hex1)+(mag_num[microsec/10]<<8);
			*HEX_ptr=(*HEX_ptr & MASK_Hex2)+(mag_num[sec%10]<<16);
			*HEX_ptr=(*HEX_ptr & MASK_Hex3)+(mag_num[sec/10]<<24);
			*HEX_ptr1=mag_num[minutes%10];
			*HEX_ptr1=(*HEX_ptr1 & MASK_Hex1)+(mag_num[minutes/10]<<8);

			return (irq_handler_t) IRQ_HANDLED;
			}


//-------------------------------------------------------------------------------------------
//Initializing function
static int __init init_TIMER(void) {

			//Configuring the virtual Memory space
			LW_Virtual= ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);	

			LED_ptr=LW_Virtual+LEDR_BASE;
			KEY_ptr=LW_Virtual+KEY_BASE;
			SW_ptr=LW_Virtual+SW_BASE; 
			HEX_ptr=LW_Virtual+HEX3_HEX0_BASE;
			HEX_ptr1=LW_Virtual+HEX5_HEX4_BASE;

			TIMER_ptr=LW_Virtual+TIMER0_BASE;


			//Looking for errors
			int err = misc_register (&LED);
			int err2 = misc_register (&KEY);
			int err3 = misc_register (&SW);
			int err4 = misc_register (&TIMER);

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
			KEY_registered = 1;
			}
			if (err3 < 0) {
			printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV3_NAME);
			return err3;
			}
			else {
			printk (KERN_INFO "/dev/%s driver registered\n", DEV3_NAME);
			SW_registered = 1;
			}
			if (err4 < 0) {
			printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV4_NAME);
			return err4;
			}
			else {
			printk (KERN_INFO "/dev/%s driver registered\n", DEV4_NAME);
			TIMER_registered = 1;
			}
			
			*(KEY_ptr + 3) = 0xF; // Clear the Edgecapture registered

			*(TIMER_ptr+2)=0x4240;
			*(TIMER_ptr+3)=0xF;
			*(TIMER_ptr+1)=7;//timer start , cnt and ito are set


			value = request_irq (72, (irq_handler_t) irq_handler, IRQF_SHARED,
				"timer0_irq_handler", (void *) (irq_handler));

			return err;
			}

//-------------------------------------------------------------------------------------------
//Deregister interrupts and the devices
static void __exit stop_TIMER(void) {
		if (LED_registered || KEY_registered || SW_registered|| TIMER_registered) 
			{
			
			misc_deregister (&LED);
			misc_deregister (&KEY);
			misc_deregister (&SW);
			misc_deregister (&TIMER);
			 *LED_ptr=0;
			*HEX_ptr=0;
			*HEX_ptr1=0;
			*(TIMER_ptr+1)=0;//de registering timer register
			*(KEY_ptr + 3) = 0xF; // Clear the Edgecapture register
			iounmap (LW_Virtual);
			free_irq (72, (void *) irq_handler);//deregistering IRQ handler for timer0
			printk (KERN_INFO "/dev/%s driver de-registered\n", DEV1_NAME);
			printk (KERN_INFO "/dev/%s driver de-registered\n", DEV2_NAME);
			printk (KERN_INFO "/dev/%s driver de-registered\n", DEV3_NAME);
			printk (KERN_INFO "/dev/%s driver de-registered\n", DEV4_NAME);
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

//-------------------------------------------------------------------------------------------
//Read and Write functions
static ssize_t device_readTIMER(struct file *filp, char *buffer, size_t length,
loff_t *offset)
		{
		sprintf(TIMER_msg,"%02d%02d%02d",minutes,sec,microsec);//converting the time variables to a string
		//printk(KERN_ERR "TIMER read:%s",TIMER_msg);
		size_t bytes;
		bytes = strlen (TIMER_msg) - (*offset);
		// how many bytes not yet sent?
		bytes = bytes > length ? length : bytes;
		// too much to send at once?
		if (bytes)
		(void) copy_to_user (buffer, &TIMER_msg[*offset], bytes);//sending the time read to the user
		// keep track of number of bytes sent to the user
		*offset = bytes;
		return bytes;
		}
//-------------------------------------------------------------------------------------------

static ssize_t device_readKEY(struct file *filp, char *buffer, size_t length, loff_t *offset)
		{
		KEY_capture=*(KEY_ptr+3);//reading the egecapture register, cleared at the end
		Button=*(KEY_ptr+3);//alternate variable to store the key press whic is not clared.
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
//-------------------------------------------------------------------------------------------
static ssize_t device_readSW(struct file *filp, char *buffer, size_t length, loff_t *offset)
		{
		SW_capture=*(SW_ptr);///reading the switch
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
//-------------------------------------------------------------------------------------------
static ssize_t device_writeTIMER(struct file *filp, const char *buffer, size_t length, loff_t *offset){
    size_t bytes;
    bytes = length;
    if (bytes > MAX_SIZE - 1)
    // can copy all at once, or not?
    bytes = MAX_SIZE - 1;
    (void) copy_from_user (CMD_value, buffer, bytes);
    CMD_value[bytes-1] ='\0'; // NULL terminate

    if(strcmp(CMD_value,end)==0)//if end command is given, stop the counter
    {
            //printk(KERN_ERR "Timer Stopped\n");
            *(TIMER_ptr+1)=11;

	}
	else if(strcmp(CMD_value,run)==0)//if run command is given start the counter
	{
		//printk(KERN_ERR"Timer Running\n");
		*(TIMER_ptr+1)=7;
	}
	else if( strlen(CMD_value)==7)//if a valid input in the form of MMSSDD+"key pres" is provided
            {
//                        
                sscanf(CMD_value,"%d",&mmssdd);//converting to integer
                mmssdd=mmssdd/10;//removing the button bit
				microsec=mmssdd%100;
                mmssdd=mmssdd/100;
                sec=mmssdd%100;
                mmssdd=mmssdd/100;
                minutes=mmssdd;
                mmssdd=0;
                if(minutes>=59)
					minutes=59;
				if(sec>=59)
					sec=59;
				if(microsec>=99)
					microsec=99;

				//printing on 7 seg display
                *HEX_ptr=mag_num[microsec%10];
				*HEX_ptr=(*HEX_ptr & MASK_Hex1)+(mag_num[microsec/10]<<8);
				*HEX_ptr=(*HEX_ptr & MASK_Hex2)+(mag_num[sec%10]<<16);
				*HEX_ptr=(*HEX_ptr & MASK_Hex3)+(mag_num[sec/10]<<24);
				*HEX_ptr1=mag_num[minutes%10];
				*HEX_ptr1=(*HEX_ptr1 & MASK_Hex1)+(mag_num[minutes/10]<<8);
			}

	else if(strlen(CMD_value)>7)//if there are more than 2 digits in any of the time variable. eg:if minutes is given as 122 by user
	{
		sscanf(CMD_value,"%d",&mmssdd);
		Button=mmssdd%10;//check which key was pressed to trigger this write
		if(Button==3)
			minutes=59;
		else if(Button==2)
			sec=59;
		else if(Button==1)
			microsec=99;

		//printing on 7 seg display
		        *HEX_ptr=mag_num[microsec%10];
				*HEX_ptr=(*HEX_ptr & MASK_Hex1)+(mag_num[microsec/10]<<8);
				*HEX_ptr=(*HEX_ptr & MASK_Hex2)+(mag_num[sec%10]<<16);
				*HEX_ptr=(*HEX_ptr & MASK_Hex3)+(mag_num[sec/10]<<24);
				*HEX_ptr1=mag_num[minutes%10];
				*HEX_ptr1=(*HEX_ptr1 & MASK_Hex1)+(mag_num[minutes/10]<<8);
	return bytes;
	}
}

//-------------------------------------------------------------------------------------------

static ssize_t device_writeLED(struct file *filp, const char *buffer, size_t length, loff_t *offset)
		{
		size_t bytes;
		bytes = length;
		if (bytes > MAX_SIZE - 1)
		bytes = MAX_SIZE - 1;
		(void) copy_from_user (LED_value, buffer, bytes);
		LED_value[bytes] ='\0'; // NULL terminate
		sscanf(LED_value,"%d",&LED_capture);
		*LED_ptr=LED_capture;
		return bytes;
		}

//-------------------------------------------------------------------------------------------
MODULE_LICENSE("GPL");
module_init (init_TIMER);
module_exit (stop_TIMER);

 