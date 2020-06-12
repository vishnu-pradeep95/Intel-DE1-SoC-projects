#include<stdio.h>
#include<string.h>
#include<signal.h>
#include<errno.h>
#include<fcntl.h>
#include<unistd.h>
#include"sprites.h"

#define BYTES 256
#define MAX_SPEED 15

#define center(x,y) ((x+y)/2)
// number of characters to read from /dev/video

void line_box(int x1,int y1,int x2,int y2,int p1, int q1, int p2, int q2,int color);
void draw_line(int x1, int y1, int x2,int y2, int color);
void v_sync(void);
void clear_screen(void);
void plot_pixel(int x, int y, int color);



volatile sig_atomic_t stop;
char command[64];
char buffer[BYTES]; // buffer for data read from /dev/video
char SW_buffer[BYTES];
char KEY_buffer[BYTES];


int VIDEO_FD,chars_read;
int KEY_FD,SW_FD;
int frames=0;
int VIDEO_val;

void catchSIGINT(int signum){
		stop = 1;
		}


int screen_x, screen_y;

int main(int argc, char *argv[])
	{
    	int row=0;
		int col=0;
		int pix=0;
		int img=0;
		int count;
		signal(SIGINT, catchSIGINT);

		// Open the character device driver
		if ((VIDEO_FD = open("/dev/VIDEO", O_RDWR)) == -1)
			{
				printf("Error opening /dev/VIDEO: %s\n", strerror(errno));
				return -1;
			}


		chars_read = 0;

		// Set screen_x and screen_y by reading from the driver
		while ((VIDEO_val = read (VIDEO_FD, buffer, BYTES)) != 0)//Reads from the switches and saves it in KEY_Buffer along with number of bytes
				{chars_read += VIDEO_val;} // read the driver until EOF
		buffer[chars_read] ='\0'; // NULL terminate
		sscanf(buffer,"x:%d,y:%d",&screen_x,&screen_y);

		clear_screen();
		v_sync();
		clear_screen();
		v_sync();
	    for(img=0;img<32;img++)
	    {   row=0;
	        col=0;
	        for(pix=1;pix<sp[img][0];pix=pix+2)
	        {
	            for(count=0;count<sp[img][pix+1];count++)
	            {
	                if(col>60)
	                {
	                    col=0;
	                    row=row+1;
	                }
	                plot_pixel(col,row,sp[img][pix]);
	                col=col+1;
	            }
		    }
		v_sync();
	    clear_screen();

	    usleep(100000);
		}
	    
		usleep(100);
		close (VIDEO_FD);
		return 0;
}

//---------------------------------------------------------------------------------------------------------------
void draw_line(int x1, int y1, int x2,int y2, int color)
{	int center_x,center_y,center_p,center_q;

	center_x=center(x1,x1+5);
	center_y=center(y1,y1+5);
	center_p=center(x2,x2+5);
	center_q=center(y2,y2+5);

	sprintf(command,"line %d,%d %d,%d %x",center_x,center_y,center_p,center_q,(color<<4));
	write(VIDEO_FD,command,strlen(command));
}

//---------------------------------------------------------------------------------------------------------------
void line_box(int x1,int y1,int x2,int y2,int p1, int q1, int p2, int q2,int color)
{
	
	int center_x,center_y,center_p,center_q;

	center_x=center(x1,x2);
	center_y=center(y1,y2);
	center_p=center(p1,p2);
	center_q=center(q1,q2);
	//Drawing box:
	sprintf(command,"box %d,%d %d,%d %x",x1,y1,x2,y2,(color<<5));
	write(VIDEO_FD,command,strlen(command));

	
	//Drawing first box2:
	sprintf(command,"box %d,%d %d,%d %x",p1,q1,p2,q2,(color<<5));
	write(VIDEO_FD,command,strlen(command));

	//drawing line connecting the boxes
	sprintf(command,"line %d,%d %d,%d %x",center_x,center_y,center_p,center_q,(color<<4));
	write(VIDEO_FD,command,strlen(command));
}
//---------------------------------------------------------------------------------------------------------------
void clear_screen(){
	
	sprintf(command,"clear ");//clearing
    //printf("CL:%s:%d\n",command,strlen(command));
	write(VIDEO_FD,command,strlen(command));
}
//---------------------------------------------------------------------------------------------------------------
void plot_pixel(int x, int y, int color){
	sprintf(command,"pixel %d,%d %x",x,y,(color<<4));
	write(VIDEO_FD,command,strlen(command));
}
//---------------------------------------------------------------------------------------------------------------
void v_sync(){

	sprintf(command,"Vsync ");//synching
	//printf("VS:%s:%d\n",command,strlen(command));
	write(VIDEO_FD,command,strlen(command));
}

//---------------------------------------------------------------------------------------------------------------