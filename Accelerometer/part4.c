#include<stdio.h>
#include<string.h>
#include<signal.h>
#include<errno.h>
#include<fcntl.h>
#include<unistd.h>

#define BYTES 256


void read_accel(void);
void v_sync(void);
void clear_screen();
void draw_box(int x1,int y1,int x2,int y2,int color);
void write_char(int VIDEO_FD,int X,int Y, int Z);
void draw_triangle(int x1,int y1,int height,int color);

volatile sig_atomic_t stop;

char command[64];
char buffer_V[BYTES]; // buffer for data read from /dev/video
char buffer_A[BYTES];
int ACCEL_val;

int ACCEL_FD;
int VIDEO_FD;
int VIDEO_val;

int screen_x, screen_y;

int X1,Y1;

void catchSIGINT(int signum){
		stop = 1;
		}
//-----------------------------------------------------------------------------------------------------
int main(int argc, char *argv[]){

	signal(SIGINT, catchSIGINT);
	int chars_read;
	int X,Y,Z,R,S;
	unsigned int H;
	//int flag=0;
	//char temp[10];

	if ((ACCEL_FD = open("/dev/ACCEL", O_RDWR)) == -1)
			{
				printf("Error opening /dev/ACCEL: %s\n", strerror(errno));
				return -1;
			}

	if ((VIDEO_FD = open("/dev/VIDEO", O_RDWR)) == -1)
			{
				printf("Error opening /dev/VIDEO: %s\n", strerror(errno));
				return -1;
			}


	chars_read = 0;


		// Set screen_x and screen_y by reading from the driver
	while ((VIDEO_val = read (VIDEO_FD, buffer_V, BYTES)) != 0)//Reads from the switches and saves it in KEY_Buffer along with number of bytes
			{chars_read += VIDEO_val;} // read the driver until EOF
	buffer_V[chars_read] ='\0'; // NULL terminate
	sscanf(buffer_V,"x:%d,y:%d",&screen_x,&screen_y);

	clear_screen();
	v_sync();

	int color=0xf800 ^ 0xffff;
	int flag=0;
	int col_disp=0xf800;
	int height=5;
	X1=160;
	Y1=120;
	chars_read = 0;
	clear_screen();
	draw_box(X1,Y1,X1+5,Y1+5,0xf800);
	v_sync();
	clear_screen();
	printf("fbsdifb\n");

	while(!stop){
	
		chars_read=0;
		while ((ACCEL_val = read (ACCEL_FD, buffer_A, BYTES)) != 0)//Reads from the switches and saves it in KEY_Buffer along with number of bytes
				{chars_read += ACCEL_val;} // read the driver until EOF
		buffer_A[chars_read] ='\0'; // NULL terminate

		//sscanf(buffer_A,"%d %d",&H1,&H2);
		sscanf(buffer_A,"%02x",&H);
		// sprintf(accel_m1, "%d %d %04d %04d %04d %d\n", intrp_source_reg,ADXL345_WasActivityUpdated(),XYZ[0], XYZ[1], XYZ[2], mg_per_lsb);
		//sscanf(buffer_A,"%x",&H);
	 	
		//f1=((H &0x40)==1) && ((H & 0x20)==0);
		//printf("%d\n",f1);
		
		//printf("YAY!!:%d\n",H);
		//single tap
		if(H & 0b00100000) {

			col_disp=col_disp^color;
			H=0;
		 	//printf("YAY!!:%02x\n",H);
		}

		if(H & 0b01000000){
			flag=!flag;
			//printf("WAY!!:%02x\n",H);
			
		}

		sscanf(buffer_A,"%*x%d%d%d",&X,&Y,&Z);
	 	//draw_box(X1,Y1,X1+fin_size,Y1+fin_size,col_disp);
	    //v_sync();
		//clear_screen();
		write_char(VIDEO_FD,X,Y,Z);
		 X1=X1+X+1;
		 Y1=Y1-Y-2;
		 if (X1>=(screen_x-5-1))
		 	X1=screen_x-6-1;
		 if(X1<=0)
		 	X1=1;
		 if(Y1>=(screen_y-5-1))
		 	Y1=screen_y-6-1;
		 if(Y1<=0)
		 	Y1=1;
		// if(R==1){
		//printf("OUT:%d,%d,%d\n",R,X,Y);
		 if(flag==0){
		draw_box(X1,Y1,X1+5,Y1+5,col_disp);
		v_sync();
		clear_screen();
		}
		else{
			draw_triangle(X1,Y1,height,col_disp);
			v_sync();
			clear_screen();
		}		
		// usleep(50000);
		
	}

	usleep(1000);
	close (ACCEL_FD);
	close(VIDEO_FD);
	return 0;


}

//---------------------------------------------------------------------------------------------------------------------------
void write_char(int VIDEO_FD,int X,int Y, int Z)
{	
	char temp[64];
	int c0=0;
	int c1=0;
	char spc=' ';

	sprintf(temp,"%03d",X);

	sprintf(command,"text %d,%d %c ",c0,c1,temp[0]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[1]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[2]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,spc);//synching
	write(VIDEO_FD,command,strlen(command));

	sprintf(temp,"%03d",Y);

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[0]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[1]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[2]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,spc);//synching
	write(VIDEO_FD,command,strlen(command));

	sprintf(temp,"%03d",Z);

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[0]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[1]);//synching
	write(VIDEO_FD,command,strlen(command));

	c0=c0+1;
	sprintf(command,"text %d,%d %c ",c0,c1,temp[2]);//synching
	write(VIDEO_FD,command,strlen(command));
}
//----------------------------------------------------------------------------------------------------------------------------
void draw_box(int x1,int y1,int x2,int y2,int color){
	//Drawing first box1:
	sprintf(command,"box %d,%d %d,%d %x",x1,y1,x2,y2,(color<<5));
	write(VIDEO_FD,command,strlen(command));
}

void read_accel(){

	int chars_read=0;
	while ((ACCEL_val = read (ACCEL_FD, buffer_A, BYTES)) != 0)//Reads from the switches and saves it in KEY_Buffer along with number of bytes
				{chars_read += ACCEL_val;} // read the driver until EOF
		buffer_A[chars_read] ='\0'; // NULL terminate

}

void clear_screen(){
	
	sprintf(command,"clear ");//clearing
    //printf("CL:%s:%d\n",command,strlen(command));
	write(VIDEO_FD,command,strlen(command));
}

void v_sync(){

	sprintf(command,"Vsync ");//synching
	//printf("VS:%s:%d\n",command,strlen(command));
	write(VIDEO_FD,command,strlen(command));
}


void draw_triangle(int x1,int y1,int height,int color){
	//Drawing first box1:
	sprintf(command,"triangle %d,%d %d %x",x1,y1,height,(color<<5));
	write(VIDEO_FD,command,strlen(command));
}
