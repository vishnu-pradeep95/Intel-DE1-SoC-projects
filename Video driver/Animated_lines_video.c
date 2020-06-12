#include<stdio.h>
#include<string.h>
#include<signal.h>
#include<errno.h>
#include<fcntl.h>
#include<unistd.h>

#define BYTES 256
#define MAX_SPEED 15

#define center(x,y) ((x+y)/2)
// number of characters to read from /dev/video

void line_box(int x1,int y1,int x2,int y2,int p1, int q1, int p2, int q2,int color);
void spirit(int *a1,int *b1,int *s1, int speed);
void draw_line(int x1, int y1, int x2,int y2, int color);
int read_SW(int SW_FD);
int read_KEY(int KEY_FD);
void write_char(int VIDEO_FD,int frame_num);

int coordinate_arr[256];
int count;
volatile sig_atomic_t stop;
char command[64];
char buffer[BYTES]; // buffer for data read from /dev/video
char SW_buffer[BYTES];
char KEY_buffer[BYTES];


int VIDEO_FD,chars_read;
int KEY_FD,SW_FD;
int frames=0;

int KEY_val,SW_val,SW_capture,KEY_capture;
int VIDEO_val;

void catchSIGINT(int signum){
		stop = 1;
		}


int screen_x, screen_y;

int main(int argc, char *argv[])
	{
    	int m=0,x1,y1,s1;
		signal(SIGINT, catchSIGINT);
		
		//int i=500;

	 	//int x1,y1,x2,y2,x3,y3,x4,y4,x5,y5,x6,y6,x7,y7,x8,y8,s1,s2,s3,s4,s5,s6,s7,s8;
	 	int sw_flag,key_flag;
	 	int speed=3;
		// Open the character device driver
		if ((VIDEO_FD = open("/dev/VIDEO", O_RDWR)) == -1)
			{
				printf("Error opening /dev/VIDEO: %s\n", strerror(errno));
				return -1;
			}
		if ((KEY_FD = open("/dev/KEY", O_RDWR)) == -1) 
				{
				printf("Error opening /dev/KEY: %s\n", strerror(errno));
				return -1;
				}

		if ((SW_FD = open("/dev/SW", O_RDWR)) == -1) 
				{
				printf("Error opening /dev/SW: %s\n", strerror(errno));
				return -1;
				}	



		chars_read = 0;

		// Set screen_x and screen_y by reading from the driver
		while ((VIDEO_val = read (VIDEO_FD, buffer, BYTES)) != 0)//Reads from the switches and saves it in KEY_Buffer along with number of bytes
				{chars_read += VIDEO_val;} // read the driver until EOF
		buffer[chars_read] ='\0'; // NULL terminate
		sscanf(buffer,"x:%d,y:%d",&screen_x,&screen_y);


        sprintf(command,"clear ");//clearing
        //printf("CL:%s:%d\n",command,strlen(command));
	    write(VIDEO_FD,command,strlen(command));
	    
	    sw_flag=read_SW(SW_FD);

		sprintf(command,"Vsync ");//synching
		//printf("VS:%s:%d\n",command,strlen(command));
		write(VIDEO_FD,command,strlen(command));
		key_flag=read_KEY(KEY_FD);

	    coordinate_arr[0]=rand()%320;
	    coordinate_arr[1]=rand()%240;
	    coordinate_arr[2]=rand()%3;

	

	    count=1;
	    while(!stop)
	    {

	    	sw_flag=read_SW(SW_FD);
			key_flag=read_KEY(KEY_FD);
			int n;

			//printf("key:%d\n\n",read_KEY(KEY_FD));
			if(key_flag==1)
				{	//printf("key1\n");
					if(speed<MAX_SPEED)
						{speed+=1;}
					else
						speed=MAX_SPEED;
				}


			if(key_flag==2)
				{	//printf("key2\n");
					if(speed>3)
						{speed-=1;}
					else
						speed=3;
				}
			if(key_flag==4)
				{
					count=count+1;
					for(n=((count-1)*3);n<=count*3;n=n+3)
						{
							coordinate_arr[n]=rand()%320;
							coordinate_arr[n+1]=rand()%240;
							coordinate_arr[n+2]=rand()%3;
						}
				}
			if(key_flag==8)
				{
					count=count-1;
					if(count==0)
						count=1;
				}
	    	for(m=0;m<count*3;m=m+3)
	    	{
	    		x1=coordinate_arr[m];
	    		y1=coordinate_arr[m+1];
	    		s1=coordinate_arr[m+2];
	    		spirit(&x1,&y1,&s1,speed);
	    		coordinate_arr[m]=x1;
	    		coordinate_arr[m+1]=y1;
	    		coordinate_arr[m+2]=s1;
	    	}
		    		sprintf(command,"clear ");//clearing
		    		write(VIDEO_FD,command,strlen(command));

	    	for(m=0;m<count*3;m=m+3)
		    	{
		    		sprintf(command,"box %d,%d %d,%d %x",coordinate_arr[m],coordinate_arr[m+1],coordinate_arr[m]+5,coordinate_arr[m+1]+5,0xf800);
					write(VIDEO_FD,command,strlen(command));
				}
					if(sw_flag && count>1)
						{
							for(m=0;m<count*3;m=m+3)
								{
									if(m+4<count*3)
										{
											draw_line(coordinate_arr[m], coordinate_arr[m+1], coordinate_arr[m+3], coordinate_arr[m+4],rand()%65535+12);
										}
									if(m+4>count*3)
										{
											draw_line(coordinate_arr[m],coordinate_arr[m+1],coordinate_arr[0],coordinate_arr[1],rand()%65535+12);
										}
								}
						
						}
				 frames=frames+1;
				 write_char(VIDEO_FD,frames);
				sprintf(command,"Vsync ");//synching
				write(VIDEO_FD,command,strlen(command));

	    	}
		usleep(100);
		close (VIDEO_FD);
		return 0;
}
//--------------------------------------------------------------------------------------------------
void write_char(int VIDEO_FD,int frame_num)
{	
	char temp[64];
	int c0=0;
	int c1=0;
	sprintf(temp,"%4d",frame_num);

	sprintf(command,"text %d,%d %c ",c0,c1,temp[0]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[1]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[2]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[3]);//synching
	write(VIDEO_FD,command,strlen(command));
}

int read_SW(int SW_FD)
{	
	int chars_read=0;
	int sw_return;
	while ((SW_val = read (SW_FD, SW_buffer, BYTES)) != 0)//Reads from the switches and saves it in SW_Buffer along with number of bytes
	{chars_read += SW_val;}// read the driver until EOF
	SW_buffer[chars_read]='\0';//NULL terminates
	chars_read = 0;

	sscanf(SW_buffer,"%d",&sw_return);
	return sw_return;
}
//---------------------------------------------------------------------------------------------------------------
int read_KEY(int KEY_FD)
{
	int chars_read=0;
	int key_return;

	while ((KEY_val = read (KEY_FD, KEY_buffer, BYTES)) != 0)//Reads from the switches and saves it in SW_Buffer along with number of bytes
	{chars_read += KEY_val;}// read the driver until EOF
	KEY_buffer[chars_read]='\0';//NULL terminates
	chars_read = 0;

	sscanf(KEY_buffer,"%d",&key_return);
	return key_return;
}

//---------------------------------------------------------------------------------------------------------------
void draw_line(int x1, int y1, int x2,int y2, int color)
{	int center_x,center_y,center_p,center_q;

	center_x=center(x1,x1+5);
	center_y=center(y1,y1+5);
	center_p=center(x2,x2+5);
	center_q=center(y2,y2+5);

	sprintf(command,"line %d,%d %d,%d %x",center_x,center_y,center_p,center_q,(color<<5));
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
	sprintf(command,"line %d,%d %d,%d %x",center_x,center_y,center_p,center_q,(color<<5));
	write(VIDEO_FD,command,strlen(command));
}
//---------------------------------------------------------------------------------------------------------------
void spirit(int *a1,int *b1,int *s1,int speed)
{

	int x1,y1,status1;
	x1=*a1;
	y1=*b1;
	status1=*s1;
	

		if(status1==0)
		{
			x1=x1+speed;
			y1=y1-speed;
			if(y1<=0 || ((y1+5)<=0))
			{
				y1=y1+speed;
				status1=1;
				//printf("ur:%d %d %d %d\n",x1,y1,x2,y2);
			}
			if(x1>=screen_x || (x1+5)>=screen_x)
			{
				x1=x1-speed;
				status1=2;
			}
		}
		if(status1==1)
		{
			x1=x1+speed;
			y1=y1+speed;
			if(x1>=screen_x || ((x1+5)>=screen_x))
			{
				x1=x1-speed;
				status1=3;
			}
			if(y1>=screen_y || (y1+5)>=screen_y)
			{
				y1=y1-speed;
				status1=0;
			}
		}
		if(status1==2)
		{
			x1=x1-speed;
			y1=y1-speed;
			if(y1<=0 || (y1+5)<=0)
			{
				y1=y1+speed;
				status1=3;
			}
			if(x1<=0 || (x1+5)<=0)
			{
				x1=x1+speed;
				status1=0;
			}
		}
		if(status1==3)
		{
			x1=x1-speed;
			y1=y1+speed;
			if(y1>=screen_y || (y1+5)>screen_y)
			{
				y1=y1-speed;
				status1=2;
			}
			if(x1<=0 || (x1+5)<=0)
			{
				x1=x1+speed;
				status1=1;
			}
		}

	*a1=x1;
	*b1=y1;
	*s1=status1;
	}
//---------------------------------------------------------------------------------------------------------------