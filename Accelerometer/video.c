#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>//for __init, see code
#include<linux/miscdevice.h>//for module init and exit macros
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include <linux/interrupt.h>
#include "../interrupt_ID.h"
#include<asm/io.h>
#include<asm/uaccess.h>
#include"address_map_arm.h"
//----------------------------------------------------------------------------------
static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);

void get_screen_specs(volatile int * pixel_ctrl_ptr);
void get_char_specs(volatile int * char_ctrl_ptr);
void clear_screen(void);
//void erase_char(void);
void plot_pixel(int x, int y, short int color);
void plot_char(int x,int y, char asci_val);
void Vsync(void);
void swap1(int *num1, int *num2);
void draw_line(int x0,int y0,int x1,int y1,short int color);
void draw_box(int x0, int y0, int x1, int y1, short int color);
void move(void);
void draw_circle(int x1, int y1, int radius, short int color);

static ssize_t device_read(struct file *filp, char *buffer,size_t length, loff_t *offset);
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);

static ssize_t device_readKEY(struct file *, char *, size_t, loff_t *);
static ssize_t device_readSW(struct file *, char *, size_t, loff_t *);


//----------------------------------------------------------------------------------
#define DEV1_NAME "VIDEO"
#define DEV2_NAME "KEY"
#define DEV3_NAME "SW"

#define SUCCESS 0
#define MAX_SIZE 256
//---------------------------------------------------------------------
static struct file_operations VIDEO_fops = {
        .owner = THIS_MODULE,
        .read = device_read,
        .write = device_write,
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

//----------------------------------------------------------------------------------------
static struct miscdevice VIDEO = {
        .minor = MISC_DYNAMIC_MINOR,
        .name = DEV1_NAME,
        .fops = &VIDEO_fops,
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
//-------------------------------------------------------------------------------------
static int VIDEO_registered = 0;
static int KEY_registered = 0;
static int SW_registered = 0;

int VIDEO_capture;
int SW_capture;
int KEY_capture;

//----------------------------------------------------------------
// Declare global variables needed to use the pixel buffer
void *LW_virtual;
int pixel_buffer,sram_virt,char_buffer;
int curr_pix_buff,curr_char_buff;
int resolution_x, resolution_y,reso_char_x,reso_char_y; // VGA screen size
int temp_pixel;

// used to access FPGA light-weight bridge
volatile unsigned int * pixel_ctrl_ptr;
volatile unsigned int * char_ctrl_ptr;

volatile int *KEY_ptr,*SW_ptr;
// virtual address of pixel buffer controller
static char VIDEO_msg[MAX_SIZE];
static char VIDEO_value[MAX_SIZE];
static char clear[MAX_SIZE]="clear";
//static char Er[MAX_SIZE]="erase";

static char KEY_msg[MAX_SIZE];
static char SW_msg[MAX_SIZE];


/* Code to initialize the video driver */
//-----------------------------------------------------------------------------
static int __init start_video(void)
{

        int err = misc_register (&VIDEO);
        int err1= misc_register (&KEY);
        int err2= misc_register (&SW);
        if (err < 0) {
                printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV1_NAME);
                return err;//returns if device registration fails
                }
                else {
                printk (KERN_INFO "/dev/%s driver registered\n", DEV1_NAME);
                VIDEO_registered = 1;
                }

        if (err1 < 0) {
                printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV2_NAME);
                return err;//returns if device registration fails
                }
                else {
                printk (KERN_INFO "/dev/%s driver registered\n", DEV2_NAME);
                KEY_registered = 1;
                 }

        if (err2 < 0) {
                printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV3_NAME);
                return err;//returns if device registration fails
                }
                else {
                printk (KERN_INFO "/dev/%s driver registered\n", DEV3_NAME);
                SW_registered = 1;
                 }


                


        LW_virtual = ioremap_nocache (0xFF200000, 0x00005000);//pixel buffer control register LW_virtual is void*
        if (LW_virtual == 0)
                printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
        pixel_ctrl_ptr=(unsigned int *)(LW_virtual+0x00003020);
        char_ctrl_ptr =(unsigned int *)(LW_virtual+0xFF203030);//pointer to character control register

        //assigning key and sw to vir mem
        KEY_ptr=LW_virtual+KEY_BASE;
        SW_ptr=LW_virtual+SW_BASE;

        get_screen_specs(pixel_ctrl_ptr);
        get_char_specs(char_ctrl_ptr);//resolution of char buffer

        pixel_buffer=(int)ioremap_nocache(0xC8000000,0x0003FFFF);
        if (pixel_buffer == 0)
                {
                printk (KERN_ERR "SDRAM Error: ioremap_nocache returned NULL\n");
                }

        sram_virt=(int)ioremap_nocache( SDRAM_BASE, FPGA_ONCHIP_SPAN);
        if (sram_virt == 0)
                {
                printk (KERN_ERR "SDRAM Error: ioremap_nocache returned NULL\n");
                                //*(pixel_ctrl_ptr + BACK_BUFFER_REGISTER) = *(pixel_ctrl_ptr + BUFFER_REGISTER);
                }
       

        char_buffer=(int)ioremap_nocache(FPGA_CHAR_BASE,FPGA_CHAR_SPAN);//character buffer
        if (char_buffer == 0)
                {
                printk (KERN_ERR "SDRAM Error: ioremap_nocache returned NULL\n");                               
                }

        *(pixel_ctrl_ptr+1)=SDRAM_BASE;//assigning back buffer mem to SDRAM
        *(KEY_ptr + 3) = 0xF; // Clear the Edgecapture register
        

        curr_char_buff=char_buffer;
        curr_pix_buff=pixel_buffer;
        
        clear_screen();
        Vsync();
         clear_screen();
         Vsync();
        //erase_char();
        return 0;
}
//-------------------------------------------------------------------------------------
void get_char_specs(volatile int * char_ctrl_ptr)
{       
        int temp_pixel;
        temp_pixel=*(char_ctrl_ptr+2);
        temp_pixel=temp_pixel<<16;
        temp_pixel=temp_pixel>>16;
        reso_char_x=temp_pixel;//first 16 bits of resolution register in pixel buffer ctrl
        reso_char_y=*(char_ctrl_ptr+2)>>16;//last 16 bits of the same.
}
//-----------------------------------------------------------------------------------
void get_screen_specs(volatile int * pixel_ctrl_ptr)
{
        temp_pixel=*(pixel_ctrl_ptr+2);
        temp_pixel=temp_pixel<<16;
        temp_pixel=temp_pixel>>16;
        resolution_x=temp_pixel;//first 16 bits of resolution register in pixel buffer ctrl
        resolution_y=*(pixel_ctrl_ptr+2)>>16;//last 16 bits of the same.
}
//-------------------------------------------------------------------------------------

void clear_screen(void)
{
        int x = 0;
        int y = 0;

        for (x = 0; x < resolution_x; x++)
                for (y = 0; y < resolution_y; y++)
                        plot_pixel(x, y, 0);
  }      
//-------------------------------------------------------------------------------------
// void erase_char(void)
// {
//  int u = 0;
//  int v = 0; 

//  for (u = 0; u < reso_char_x; u++)
//         {for (v = 0; v < reso_char_y; v++)
//                { plot_char(u, v,' ');}
//           }     
// } 

//-------------------------------------------------------------------------------------

void plot_pixel(int x,int y, short int color)
{
        short int* pixel = (short int*)(curr_pix_buff + ((x & 0x1FF)<<1)+((y &0xFF)<<10));
        *pixel=color;
}
//---------------------------------------------------------------------------------------
//to plot characters
void plot_char(int x,int y, char asci_val)
{
        short int* chara = (short int*)(curr_char_buff + x+(y<<7));
        *chara=asci_val;
}
//-------------------------------------------------------------------------------------

void Vsync(void)
{
        *(pixel_ctrl_ptr)=0x1;
        while(*(pixel_ctrl_ptr+3)&1)
        {

        }
        if(*(pixel_ctrl_ptr)==SDRAM_BASE)
                curr_pix_buff=pixel_buffer;
        else
                curr_pix_buff=sram_virt;
}

//-------------------------------------------------------------------------------------

void swap1(int *num1, int *num2)
{
    int temp;
    temp=*num1;
    *num1=*num2;
    *num2=temp;
}
//-------------------------------------------------------------------------------------
void draw_triangle(int x1, int y1, int height, short int color){

	int i;

	plot_pixel(x1,y1,color);
	for(i=0;i<=height;i++){
		draw_line(x1-i,y1+i,x1+i,y1+i,color);
	}
}

void draw_line(int x0, int y0, int x1, int y1, short int color)
{
    int deltax = 0;
    int deltay = 0;
    int error = 0;
    int x = 0;
    int y = 0;
    int y_step = 0;
    int is_steep = (abs(y1-y0) > abs(x1-x0));

    if (x0 > resolution_x)
        x0 = resolution_x;
    if (x1 > resolution_x)
        x1 = resolution_x;
    if (y0 > resolution_y)
        y0 = resolution_y;
    if (y1 > resolution_y)
        y1 = resolution_y;

    
    if (is_steep)
    {
        swap1(&x0, &y0);
        swap1(&x1, &y1);
    }
    if (x0 > x1)
    {
        swap1(&x0, &x1);
        swap1(&y0, &y1);
    }
    
    deltax = x1 - x0;
    deltay = abs(y1 - y0);
    error = -(deltax / 2);
    y = y0;
    
    if (y0 < y1)
        y_step = 1;
    else
        y_step = -1;
    
    for (x = x0; x <= x1; x++)
    {
        if (is_steep)
            plot_pixel(y, x, color);
        else
            plot_pixel(x, y, color);
        
        error = error + deltay;
        
        if (error > 0)
        {
            y = y + y_step;
            error = error - deltax;
        }
    }
        
}
//-------------------------------------------------------------------------------------

void draw_box(int x0, int y0, int x1, int y1, short int color)
{
    int i = 0;

    if (x0 > resolution_x)
        x0 = resolution_x;
    if (x1 > resolution_x)
        x1 = resolution_x;
    if (y0 > resolution_y)
        y0 = resolution_y;
    if (y1 > resolution_y)
        y1 = resolution_y;

    if (y0 > y1)
        swap1(&y1, &y0);

    for(i=y0; i<=y1; i++){
        draw_line(x0, i, x1, i, color);
    }
}

//-------------------------------------------------------------------------------------
static void __exit stop_video(void)
{
/* unmap the physical-to-virtual mappings */
    iounmap (LW_virtual);
    iounmap ((void *) pixel_buffer);
    iounmap ((void *) sram_virt);
    iounmap ((void *) char_buffer);

    VIDEO_registered = 0;
    KEY_registered = 0;
    SW_registered = 0;
    KEY_ptr=NULL;
    curr_char_buff=0;
    curr_pix_buff=0;
    clear_screen();
    Vsync();
    clear_screen();

/* Remove the device from the kernel */
    misc_deregister (&VIDEO);
    misc_deregister (&KEY);
    misc_deregister (&SW);
    printk (KERN_INFO "/dev/%s driver de-registered\n", DEV1_NAME);

}
//-------------------------------------------------------------------------------------
static int device_open(struct inode *inode, struct file *file)
{
     return SUCCESS;
}
static int device_release(struct inode *inode, struct file *file)
{
     return 0;
}
//-------------------------------------------------------------------------------------
static ssize_t device_read(struct file *filp, char *buffer,size_t length, loff_t *offset)//sends the resolution to the user.
{
        //VIDEO_capture=(resolution_y,resolution_y);//read the resolution register
        sprintf(VIDEO_msg,"x:%d,y:%d\n",resolution_x,resolution_y);
        size_t bytes;
        bytes = strlen (VIDEO_msg) - (*offset);
        // how many bytes not yet sent?
        bytes = bytes > length ? length : bytes;
        // too much to send at once?
        if (bytes)
        (void) copy_to_user (buffer, &VIDEO_msg[*offset], bytes);
        // keep track of number of bytes sent to the user
        *offset = bytes;
        return bytes;
}
//-----------------------------------------------------------------------------------------
static ssize_t device_readKEY(struct file *filp, char *buffer, size_t length, loff_t *offset)
        {
        KEY_capture=*(KEY_ptr+3);//reading the egecapture register, cleared at the end
        
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
//-------------------------------------------------------------------------------------
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
        
        return bytes;
        }
//----------------------------------------------------------------------------------------
static ssize_t device_write(struct file *filp, const char
                            *buffer, size_t length, loff_t *offset)
{
        int len,value,p,q,height,color,asci_val;
        int x1,y1,x2,y2;
        size_t bytes;
        bytes = length;
        if (bytes > MAX_SIZE - 1)
        bytes = MAX_SIZE - 1;
        (void) copy_from_user (VIDEO_value, buffer, bytes);
        VIDEO_value[bytes-1] ='\0'; // NULL terminate
        //sscanf(VIDEO_value,"%d",&VIDEO_capture);
        //printk(KERN_ERR "VIDEO_value:%s\n",VIDEO_value);
        value=strcmp(VIDEO_value,clear);
        len=strlen(VIDEO_value);
        //printk(KERN_ERR "length:%d\n",len);
        //printk(KERN_ERR "value:%d\n",value);
        if(strcmp(VIDEO_value,clear)==0)//if the user sends the clear command, clear the screen.
                {   //printk(KERN_ERR"clearing\n");
                    clear_screen();}

        if(sscanf(VIDEO_value,"pixel %d,%d %x",&p,&q,&color)==3)//see sscanf return values
                {       //printk(KERN_ERR "plotting:\n");
                        plot_pixel(p,q,color);
                }

        if(strcmp(VIDEO_value,"Vsync")==0)//Vsync called when Vsyn command is given
                {   //printk(KERN_ERR"synching\n");
                    Vsync();
                }

        if(sscanf(VIDEO_value,"line %d,%d %d,%d %x",&x1,&y1,&x2,&y2,&color)==5)
                {
                    draw_line(x1,y1,x2,y2,color);
                }

        if(sscanf(VIDEO_value,"box %d,%d %d,%d %x",&x1,&y1,&x2,&y2,&color)==5)
                {
                    draw_box(x1,y1,x2,y2,color);
                }

        if(sscanf(VIDEO_value,"text %d,%d %c",&x1,&y1,&asci_val)==3)
                {   //printk(KERN_ERR"plotting chara\n");
                    plot_char(x1,y1,asci_val);
                }

        if(sscanf(VIDEO_value,"triangle %d,%d %d %x",&x1,&y1,&height,&color)==4){
        	draw_triangle(x1,y1,height,color);
        }

        // if(strcmp(VIDEO_value,Er)==0)//if the user sends the clear command, clear the screen.
        //         {   //printk(KERN_ERR"clearing\n");
        //             erase_char();
        //         }

        return bytes;
}
//-------------------------------------------------------------------------------------

MODULE_LICENSE("GPL");
module_init (start_video);
module_exit (stop_video);
