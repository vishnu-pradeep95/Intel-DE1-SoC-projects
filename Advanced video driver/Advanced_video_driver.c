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
#include"../address_map_arm.h"
#include"sprites.h"
#include<linux/random.h>
#include<linux/delay.h> 
//----------------------------------------------------------------------------------
static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);

void get_screen_specs(volatile int * pixel_ctrl_ptr);
void clear_screen(void);
void plot_pixel(int x, int y, short int color);
void Vsync(void);
void swap1(int *num1, int *num2);
void draw_line(int x0,int y0,int x1,int y1,short int color);
void draw_box(int x0, int y0, int x1, int y1, short int color);
void move(void);
void draw_curve(int x0, int y0, int x1, int y1, int x2, int y2, short int color);
void figure(void);
void fill_poly(int number ,int x0,int y0);
void draw_poly(int number,int x1,int y1);

static ssize_t device_read(struct file *filp, char *buffer,size_t length, loff_t *offset);
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);



//----------------------------------------------------------------------------------
#define DEV1_NAME "VIDEO"

#define SUCCESS 0
#define MAX_SIZE 256
#define FRAME_NO 32
#define ROW_MID 60
#define COL_MID 130 
//---------------------------------------------------------------------
static struct file_operations VIDEO_fops = {
        .owner = THIS_MODULE,
        .read = device_read,
        .write = device_write,
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
//-------------------------------------------------------------------------------------
static int VIDEO_registered = 0;
int VIDEO_capture;

//----------------------------------------------------------------
// Declare global variables needed to use the pixel buffer
void *LW_virtual;
int pixel_buffer,sram_virt;
int curr_buff;
int resolution_x, resolution_y; // VGA screen size
int temp_pixel;

// used to access FPGA light-weight bridge
volatile unsigned int * pixel_ctrl_ptr;

// virtual address of pixel buffer controller
static char VIDEO_msg[MAX_SIZE];
static char VIDEO_value[MAX_SIZE];
static char clear[MAX_SIZE]="clear";

/* Code to initialize the video driver */
//-----------------------------------------------------------------------------
static int __init start_video(void)
{

        int err = misc_register (&VIDEO);

        if (err < 0) {
                printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV1_NAME);
                goto err_out;//returns if device registration fails
                }
                else {
                printk (KERN_INFO "/dev/%s driver registered\n", DEV1_NAME);
                VIDEO_registered = 1;
                 }

        LW_virtual = ioremap_nocache (0xFF200000, 0x00005000);//pixel buffer control register LW_virtual is void*
        if (LW_virtual == 0)
                printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
        pixel_ctrl_ptr=(unsigned int *)(LW_virtual+0x00003020);

        get_screen_specs(pixel_ctrl_ptr);

        pixel_buffer=(int)ioremap_nocache(0xC8000000,0x0003FFFF);
        if (pixel_buffer == 0)
                {
                printk (KERN_ERR "VIrtual MEM Error: ioremap_nocache returned NULL\n");
                                //*(pixel_ctrl_ptr + BACK_BUFFER_REGISTER) = *(pixel_ctrl_ptr + BUFFER_REGISTER);
                }

        sram_virt=(int)ioremap_nocache( SDRAM_BASE, FPGA_ONCHIP_SPAN);
        if (sram_virt == 0)
                {
                printk (KERN_ERR "SDRAM Error: ioremap_nocache returned NULL\n");
                                //*(pixel_ctrl_ptr + BACK_BUFFER_REGISTER) = *(pixel_ctrl_ptr + BUFFER_REGISTER);
                }
        *(pixel_ctrl_ptr+1)=SDRAM_BASE;

        curr_buff=pixel_buffer;
       
        clear_screen();
        Vsync();
        clear_screen();
        Vsync();
        
        return 0;

        err_out:
        return err;
}
//-------------------------------------------------------------------------------------
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

void plot_pixel(int x,int y, short int color)
{
        short int* pixel = (short int*)(curr_buff + ((x & 0x1FF)<<1)+((y &0xFF)<<10));
        *pixel=color;
}
//-------------------------------------------------------------------------------------

void Vsync(void)
{
        *(pixel_ctrl_ptr)=0x1;
        while(*(pixel_ctrl_ptr+3)&1)
        {
            //wait
        }
        if(*(pixel_ctrl_ptr)==SDRAM_BASE)
                curr_buff=pixel_buffer;
        else
                curr_buff=sram_virt;
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
void draw_curve2(int x0, int y0, int x1, int y1, int x2, int y2, short int color){

    /*Bezier curve of order 2 is drawn using the code that is given below. Usually 
    bezier curve is drawn using floating point numbers but kernel module does not 
    support we have implemented it in integers*/
    int Px,Py;
    int Prev_Px,Prev_Py;
    Prev_Px=x0;
    Prev_Py=y0;
    int t;
    draw_line(x0,y0,x1,y1,color);
    draw_line(x1,y1,x2,y2,color);

    t=0;
    while(t<=100){
        //multilpying each t value by 100 to circumnavigate float arithmeitic (change the number to an integer with last 2 places having the extra decimal place values )
        //and removing the same factor after multiplying them with the control points
        Px=(((100-t)*(100-t)*x0)/10000)+((2*(100-t)*t*x1)/10000)+((t*t*x2)/10000);
        Py=(((100-t)*(100-t)*y0)/10000)+((2*(100-t)*t*y1)/10000)+((t*t*y2)/10000);
        if(Prev_Px==Px){//sometimes the values that we get using the baove formula will differ only in the decimal place
            if(Prev_Py==Py){//since we do not have decimal place, the values will be the same and line cannot be drawn using two same points
            	t=t+1;
                continue;
            }
        }
        else{
            draw_line(Prev_Px,Prev_Py,Px,Py,color);
            Prev_Px=Px;
            Prev_Py=Py;
        }

        //printf("Px=%d, Py=%d, t=%d\n",Px,Py,t);
        t=t+1;
    }
}
//-------------------------------------------------------------------------------------
void draw_curve3(int x0, int y0, int x1, int y1, int x2, int y2, int x3,int y3,short int color){
    /*Bezier curve of order 3 is drawn using the code that is given below. Usually 
    bezier curve is drawn using floating point numbers but kernel module does not 
    support we have implemented it in integers*/
    int Px,Py;
    int Prev_Px,Prev_Py;
    Prev_Px=x0;
    Prev_Py=y0;
    int t;
    draw_line(x0,y0,x1,y1,color);
    draw_line(x1,y1,x2,y2,color);
    draw_line(x2,y2,x3,y3,color);

    t=0;
    while(t<=100){
        //multilpying each t value by 100 to circumnavigate float arithmeitic (change the number to an integer with last 2 places having the extra decimal place values )
        //and removing the same factor after multiplying them with the control points
        Px=(((100-t)*(100-t)*(100-t)*x0)/1000000)+((3*(100-t)*(100-t)*t*x1)/1000000)+((3*(100-t)*t*t*x2)/1000000)+((t*t*t*x3)/1000000);
        Py=(((100-t)*(100-t)*(100-t)*y0)/1000000)+((3*(100-t)*(100-t)*t*y1)/1000000)+((3*(100-t)*t*t*y2)/1000000)+((t*t*t*y3)/1000000);
        if(Prev_Px==Px){//sometimes the values that we get using the baove formula will differ only in the decimal place
            if(Prev_Py==Py){//since we do not have decimal place, the values will be the same and line cannot be drawn using two same points
                t=t+1;
                continue;
            }
        }
        else{
            draw_line(Prev_Px,Prev_Py,Px,Py,color);
            Prev_Px=Px;
            Prev_Py=Py;
        }

        t=t+1;
    }
}
//-------------------------------------------------------------------------------------
void figure(void){
    /*This function is used to display an animated sprite whose values are subjected to run-length encoding
    and had been presented in sprites.h file.*/
	int row=0;
	int col=0;
	int pix=0;
	int img=0;
    int value,count;
    clear_screen();
	Vsync();
	clear_screen();
    for(img=0;img<FRAME_NO;img++)//running the loop based on the number of frames present
    {   row=0;
        col=0;
        for(pix=1;pix<sp[img][0];pix=pix+2)//running the specific frame for the number of times given as the first element in each dimension
        {
            for(count=0;count<sp[img][pix+1];count++)//plotting each hex-color for the specific count
            {
                if(col>60)
                {
                    col=0;
                    row=row+1;//changing to next row when the current row hits the end at column = 61
                }
                plot_pixel(col+COL_MID,row+ROW_MID,sp[img][pix]);//plotting the pixels from sprites.h in the middle of the frame using the parameters
                col=col+1;
            }
	    }
	Vsync();
    clear_screen();
    mdelay(100);//obtained from /linux/delay.h
	}
}
//-------------------------------------------------------------------------------------
void fill_poly(int n ,int x0,int y0){
    int i,j,k,dy,dx,y,temp;
    int randv,randh;
    int poly_vert[20][2],xi[2400];//the x and y coordinate for each point is stored in a 2d array
    int slope_inv[20];
    //store the coordiantes of first point
    poly_vert[0][0]=x0;
    poly_vert[0][1]=y0;
    i=1;
    /*this loop generates all the other points reqired for the cmolpex polygon randomly
    by using the get_random_bytes() function in "/linux/random.h".
    */
    while(i<n){
            //x co-ordinate
            get_random_bytes(&randh,sizeof(randv));//getting a random value to choose arbitrary side lengths
            randh&=0x7FFFFFFF;//masking the first bit to only accept positive numbers
            randh=randh%(320-x0-10);
            poly_vert[i][0]=x0+randh;

            //y co-ordinate
            get_random_bytes(&randv,sizeof(randv));
            randv&=0x7FFFFFFF;
            randv=randv%(240-y0-10);
            poly_vert[i][1]=y0+randv;
            i++;
     }
    //the nth index is stored with the first coordinate to form a closed graph
     poly_vert[n][0]=poly_vert[0][0];
     poly_vert[n][1]=poly_vert[0][1];

     //all the vertices of the polygon are connected
     for(i=0;i<n;i++){
        draw_line(poly_vert[i][0],poly_vert[i][1],poly_vert[i+1][0],poly_vert[i+1][1],0xf800);
     }
    //finding the slope_invs of the edges of the polygon
     for(i=0;i<n;i++){
        dy=poly_vert[i+1][1]-poly_vert[i][1];
        dx=poly_vert[i+1][0]-poly_vert[i][0];

        if(dy==0) slope_inv[i]=1;
        if(dx==0) slope_inv[i]=0;

        if((dy!=0) && (dx!=0))
        {
            //multilpying by 100 to circumnavigate float arithmeitic (change the number to an integer with last 2 places having the extra decimal place values )
            slope_inv[i]= dx*100/dy;
        }
     }
  
    //to calculate the interscetion of the scan line  and the polygon
    //Here the y variable is the horizontal scan line 
    for(y=0;y<240;y++){
        k=0;
        /*Looping through the diff edges of the polygon
        This is done for each of the horizontal scan lines
        */
        for(i=0;i<n;i++){
            if(((poly_vert[i][1]<=y) && (poly_vert[i+1][1]>y)) || ((poly_vert[i][1]>y) && (poly_vert[i+1][1]<=y))){
                
                /*store the intersection points
                x coordinates of the intersection between scan line and the edge is stored by increment the x coordinate by dx (slope_inv)
                */
                xi[k]=(int)(poly_vert[i][0]+(slope_inv[i]*(y-poly_vert[i][1])/100));//dividing by 100 to circumnavigate float arithmeitic
                k++;//for the next intersection points
            }
        }
        
        for(j=0;j<k-1;j++)
        //sorting
        for(i=0;i<k-1;i++){
            if(xi[i]>xi[i+1]){
                temp=xi[i];
                xi[i]=xi[i+1];
                xi[i+1]=temp;
            }
        }
        //filling the area between intersection points on the scan line with the required color 
        for(i=0;i<k;i=i+2){
            draw_line(xi[i],y,xi[i+1]+1,y,0xf800);
        }
    }
}
//-------------------------------------------------------------------------------------

void draw_poly(int n ,int x0,int y0){
    int i;
    int randv,randh;

    int poly_vert[20][2],xi[2400];//the x and y coordinate for each point is stored in a 2d array


    //store the coordiantes of first point
    poly_vert[0][0]=x0;
    poly_vert[0][1]=y0;

    i=1;
    /*this loop generates all the other points reqired for the cmolpex polygon randomly
    by using the get_random_bytes() function in "/linux/random.h".
    */
    while(i<n){
            //x co-ordinate
            get_random_bytes(&randh,sizeof(randv));//getting a random value to choose arbitrary side lengths
            randh&=0x7FFFFFFF;//masking the first bit to only accept positive numbers
            randh=randh%(320-x0-10);
            poly_vert[i][0]=x0+randh;

            //y co-ordinate
            get_random_bytes(&randv,sizeof(randv));
            randv&=0x7FFFFFFF;
            randv=randv%(240-y0-10);
            poly_vert[i][1]=y0+randv;
            i++;
     }

    //the nth index is stored with the first coordinate to form a closed graph
     poly_vert[n][0]=poly_vert[0][0];
     poly_vert[n][1]=poly_vert[0][1];

     //all the vertices of the polygon are connected
     for(i=0;i<n;i++){
        draw_line(poly_vert[i][0],poly_vert[i][1],poly_vert[i+1][0],poly_vert[i+1][1],0xf800);
     }
}
//-------------------------------------------------------------------------------------
static void __exit stop_video(void){
/* unmap the physical-to-virtual mappings */
    iounmap (LW_virtual);
    iounmap ((void *) pixel_buffer);
    iounmap ((void *) sram_virt);
    VIDEO_registered = 1;

/* Remove the device from the kernel */
    misc_deregister (&VIDEO);
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
//-------------------------------------------------------------------------------------
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
        int len,value,p,q,color,number;
        int x0,y0,x1,y1,x2,y2,x3,y3;
        size_t bytes;
        bytes = length;
        if (bytes > MAX_SIZE - 1)
        bytes = MAX_SIZE - 1;
        (void) copy_from_user (VIDEO_value, buffer, bytes);
        VIDEO_value[bytes-1] ='\0'; // NULL terminate
       
        value=strcmp(VIDEO_value,clear);
        len=strlen(VIDEO_value);
       
        if(strcmp(VIDEO_value,clear)==0)//if the user sends the clear command, clear the screen.
                {   
                    clear_screen();
                }

        if(sscanf(VIDEO_value,"pixel %d,%d %x",&p,&q,&color)==3)//see sscanf return values
                {       
                        plot_pixel(p,q,color);
                }

        if(strcmp(VIDEO_value,"Vsync")==0)//Vsync called when Vsyn command is given
                {   
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

        if(sscanf(VIDEO_value,"curve2 %d,%d %d,%d %d,%d %x",&x0,&y0,&x1,&y1,&x2,&y2,&color)==7)
                {	
                    draw_curve2(x0,y0,x1,y1,x2,y2,color);
                }
        if(sscanf(VIDEO_value,"curve3 %d,%d %d,%d %d,%d %d,%d %x",&x0,&y0,&x1,&y1,&x2,&y2,&x3,&y3,&color)==9)
                {   
                    draw_curve3(x0,y0,x1,y1,x2,y2,x3,y3,color);
                }
        if(strcmp(VIDEO_value,"sprite")==0)
                {   
                    figure();
                }
        if(sscanf(VIDEO_value,"polygon_fills %d %d,%d",&number,&x1,&y1)==3)
                {
                    fill_poly(number,x1,y1);
                }
        if(sscanf(VIDEO_value,"polygon_draws %d %d,%d",&number,&x1,&y1)==3)
                {
                    draw_poly(number,x1,y1);
                }
        
        return bytes;
}
//-------------------------------------------------------------------------------------

MODULE_LICENSE("GPL");
module_init (start_video);
module_exit (stop_video);
