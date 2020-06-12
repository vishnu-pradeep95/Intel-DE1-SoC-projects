#define _GNU_SOURCE

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "../address_map_arm.h"
#include <unistd.h> 
#include <math.h>
#include <string.h>
#include <linux/input.h>
#include <pthread.h>
#include <sched.h>
#include<errno.h>

/* Prototypes for functions used to access physical memory addresses */
int open_physical (int);
void * map_physical (int, unsigned int, unsigned int);
void close_physical (int);
int unmap_physical (void *, unsigned int);



#define SAMPLE_RATE 8000
#define PI 3.14159265
#define PI2 6.28318531
#define MAX_VOLUME 0x7fffffff
#define KEY_RELEASED 0
#define KEY_PRESSED 1
#define BYTES 256

int RED_VOL=0xFD89D89;


void clear_coeff();
void assign_coeff();
void reset_tones();

int flag_samp=0;	
double freq[13] = {261.626, 277.183, 293.665, 311.127,329.628, 349.228,369.994, 391.995, 415.305, 440, 466.164, 493.883, 523.251};
int co_eff[14]={1,1,1,1,1,1,1,1,1,1,1,1,1,1};//coeff of sine sums
int audio_write[2500];
int tone=0;
int nth_sample=0;

int tone_record[2500];//records the key press
int press_record[2500];//records the time at key press
int release_record[2500];// records the time at key release

int curr_min,curr_sec,curr_microsec;

int tone_volume[14]={0,0,0,0,0,0,0,0,0,0,0,0,0,0};//volume of each key
int tone_index=14,prev_press=14;//14 index is invalid case
int key_capture,flag=0,no_keys=0;

volatile int * AUDIO_ptr; 
pthread_mutex_t mutex_tone_volume;// mutex for main and audio_thread
int audio_write[2500];
pthread_mutex_t mutex_audio_write;


//Video Functions
int chars_read;
int screen_x, screen_y;
int VIDEO_FD;
int VIDEO_val;
char buffer_V[BYTES]; // buffer for data read from /dev/video
char command[64];
void v_sync(void);
void clear_screen(void);
void draw_line(int x1,int y1,int x2,int y2,int color);

//KEY Functions
int KEY_FD,KEY_capture,KEY_val;
int chars_read;
char KEY_buffer[BYTES];
int key_flag=0;

//LED
int LED_FD;
//TIMER
int TIMER_FD,TIMER_val,TIMER_capture;
char TIMER_buffer[BYTES];
char TIMER_ref[BYTES];
//------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Set the current thread’s affinity to the core specified
int set_processor_affinity(unsigned int core){
	cpu_set_t cpuset;
	pthread_t current_thread = pthread_self();

	if (core >= sysconf(_SC_NPROCESSORS_ONLN)){
		printf("CPU Core %d does not exist!\n", core);
		return -1;
	}
	// Zero out the cpuset mask
	CPU_ZERO(&cpuset);
	// Set the mask bit for specified core
	CPU_SET(core, &cpuset);
	return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t),&cpuset);
}
//------------------------------------------------------------------
void *audio_thread(){

	set_processor_affinity(0);
	int fd1 = -1;
	void *LW_virtual; // physical addresses for light-weight bridge

	// Create virtual memory access to the FPGA light-weight bridge
	if ((fd1 = open_physical (fd1)) == -1)
		return (-1);
	if (!(LW_virtual = map_physical (fd1, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)))
		return (-1);


	// Set virtual address pointer to I/O port
	AUDIO_ptr = (int *) (LW_virtual + AUDIO_BASE);
	//clearing the fifo buffer by using CW bit
	*AUDIO_ptr = 0x00000008;
	*AUDIO_ptr = 0x00000000;
	//**********************
	while(1){
		// Check if this thread has been cancelled.
		// If it was, the thread will halt at this point.
		pthread_testcancel();
		
		 	while((((*(AUDIO_ptr+1)>>16) & 0xFF) <0x40) && (((*(AUDIO_ptr+1)>>24) & 0xFF) <0x40));

		 	tone=( ( (co_eff[0]*tone_volume[0]*sin(nth_sample*PI2 *  freq[0]/SAMPLE_RATE)) + (co_eff[1]*tone_volume[1]*sin(nth_sample*PI2 *  freq[1]/SAMPLE_RATE))+
									(co_eff[2]*tone_volume[2]*sin(nth_sample*PI2 *  freq[2]/SAMPLE_RATE)) + (co_eff[3]*tone_volume[3]*sin(nth_sample*PI2 *  freq[3]/SAMPLE_RATE))+
									(co_eff[4]*tone_volume[4]*sin(nth_sample*PI2 *  freq[4]/SAMPLE_RATE)) + (co_eff[5]*tone_volume[5]*sin(nth_sample*PI2 *  freq[5]/SAMPLE_RATE))+
									(co_eff[6]*tone_volume[6]*sin(nth_sample*PI2 *  freq[6]/SAMPLE_RATE)) + (co_eff[7]*tone_volume[7]*sin(nth_sample*PI2 *  freq[7]/SAMPLE_RATE))+
									(co_eff[8]*tone_volume[8]*sin(nth_sample*PI2 *  freq[8]/SAMPLE_RATE)) + (co_eff[9]*tone_volume[9]*sin(nth_sample*PI2 *  freq[9]/SAMPLE_RATE))+
									(co_eff[10]*tone_volume[10]*sin(nth_sample*PI2 *  freq[10]/SAMPLE_RATE)) + (co_eff[11]*tone_volume[11]*sin(nth_sample*PI2 *  freq[11]/SAMPLE_RATE))+
									(co_eff[12]*tone_volume[12]*sin(nth_sample*PI2 *  freq[12]/SAMPLE_RATE)) )     );

		 	pthread_mutex_lock (&mutex_tone_volume);
		 	tone_volume[tone_index]*=0.9999;
		
			if(no_keys>1){
				tone_volume[prev_press]*=0.9999;//reduction of volume
			}
			pthread_mutex_unlock (&mutex_tone_volume);
			nth_sample++;
			*(AUDIO_ptr+2)=tone;//left data
		 	*(AUDIO_ptr+3)=tone;//right data
	
	}
		unmap_physical (LW_virtual, LW_BRIDGE_SPAN);
	close_physical (fd1);
}
//-----------------------------------------------------------------------------------------------------------

void *video_thread(){

	set_processor_affinity(1);
	int x_value,y_value;
	int x_prev,y_prev;

	x_prev=0;
	x_value=0;

	if ((VIDEO_FD = open("/dev/VIDEO", O_RDWR)) == -1)
		{
			printf("Error opening /dev/VIDEO: %s\n", strerror(errno));
			return -1;
		}


		// Set screen_x and screen_y by reading from the driver
	while ((VIDEO_val = read (VIDEO_FD, buffer_V, BYTES)) != 0)//Reads from the switches and saves it in KEY_Buffer along with number of bytes
			{chars_read += VIDEO_val;} // read the driver until EOF
	buffer_V[chars_read] ='\0'; // NULL terminate
	sscanf(buffer_V,"x:%d,y:%d",&screen_x,&screen_y);



	y_prev=screen_y/2;

    clear_screen();
    draw_line(0,0,0,screen_y,0xffff);
	draw_line(0,screen_y/2,screen_x,screen_y/2,0xffff);	
    v_sync();


	while(1){
		
		if(no_keys){
			clear_screen();//clears the screen when a key is pressed and the wave is generated
			draw_line(0,0,0,screen_y,0xffff);
			draw_line(0,screen_y/2,screen_x,screen_y/2,0xffff);	
		

			while(x_value<screen_x){
				y_value=(int)(screen_y/2)*(((co_eff[0]*tone_volume[0]*sin(x_value*PI2 *  freq[0]/SAMPLE_RATE)) + (co_eff[1]*tone_volume[1]*sin(x_value*PI2 *  freq[1]/SAMPLE_RATE))+
											(co_eff[2]*tone_volume[2]*sin(x_value*PI2 *  freq[2]/SAMPLE_RATE)) + (co_eff[3]*tone_volume[3]*sin(x_value*PI2 *  freq[3]/SAMPLE_RATE))+
											(co_eff[4]*tone_volume[4]*sin(x_value*PI2 *  freq[4]/SAMPLE_RATE)) + (co_eff[5]*tone_volume[5]*sin(x_value*PI2 *  freq[5]/SAMPLE_RATE))+
											(co_eff[6]*tone_volume[6]*sin(x_value*PI2 *  freq[6]/SAMPLE_RATE)) + (co_eff[7]*tone_volume[7]*sin(x_value*PI2 *  freq[7]/SAMPLE_RATE))+
											(co_eff[8]*tone_volume[8]*sin(x_value*PI2 *  freq[8]/SAMPLE_RATE)) + (co_eff[9]*tone_volume[9]*sin(x_value*PI2 *  freq[9]/SAMPLE_RATE))+
											(co_eff[10]*tone_volume[10]*sin(x_value*PI2 *  freq[10]/SAMPLE_RATE)) + (co_eff[11]*tone_volume[11]*sin(x_value*PI2 *  freq[11]/SAMPLE_RATE))+
											(co_eff[12]*tone_volume[12]*sin(x_value*PI2 *  freq[12]/SAMPLE_RATE)))/RED_VOL);

				y_value+=screen_y/2;//the wave is started from the middle of the screen
				if(y_value>screen_y-1){
					y_value=screen_y-1;//setting the maximmum value within the screen resolution
				}
				if(y_value<1){
					y_value=1;//setting the minimum value within the screen resolution
				}
				draw_line(x_prev,y_prev,x_value,y_value,0xf80);

				x_value++;

				x_prev=x_value;//setting the current value as the previous value for the continuation of line drawing
				y_prev=y_value;
			}
			v_sync();
			x_prev=0;
			x_value=0;
			y_prev=screen_y/2;
			y_value=0;
			}
		}
	close(VIDEO_FD);

}


//-----------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[]){
	
	
	int fd2; // used to open /dev/mem
	set_processor_affinity(1);

	struct input_event ev;
	int event_size = sizeof (struct input_event); 

	//multuthreading variables
	pthread_t tid_aud,tid_vid;
	int err,err2;
	int i=0;
	int j=0;
	// Spawn the audio thread.
	if ((err = pthread_create(&tid_aud, NULL, &audio_thread, NULL)) != 0){
		printf("pthread_create failed:[%s]\n", strerror(err));
	}
	//Spaw the Video thread
	if ((err2 = pthread_create(&tid_vid, NULL, &video_thread, NULL)) != 0){
		printf("pthread_create failed:[%s]\n", strerror(err2));
	}

	// Get the keyboard device
	if (argv[1] == NULL){
		printf("Specify the path to the keyboard device ex./dev/input/by-id/usb-Logitech_USB_Keyboard-event-kbd");
		return -1;
	}


	// Open keyboard device
	if ((fd2 = open (argv[1], O_RDONLY | O_NONBLOCK)) == -1){
		printf ("Could not open %s\n", argv[1]);
		return -1;
	}

	//Open Key
	// Open the character device driver for read/write
	if ((KEY_FD = open("/dev/KEY", O_RDWR)) == -1) {
		printf("Error opening /dev/KEY: %s\n", strerror(errno));
		return -1;
	}

	//open LED
	if ((LED_FD = open("/dev/LED", O_RDWR)) == -1){
		printf("Error opening /dev/LED: %s\n", strerror(errno));
		return -1;
	}

	//open timer
	if ((TIMER_FD = open("/dev/TIMER", O_RDWR)) == -1){
		printf("Error opening /dev/TIMER: %s\n", strerror(errno));
		return -1;
	}
	//********************
	clear_coeff();
	tone_index=14;
	prev_press=14;
	write(LED_FD,"0",strlen("0"));
	write (TIMER_FD,"end", (strlen("end"))+1);// stop the timer initially
	write(TIMER_FD,"0059991",(strlen("0100001"))+1);

	while (1){
	// Read keyboard
		chars_read = 0;
		while ((KEY_val = read (KEY_FD, KEY_buffer, BYTES)) != 0)//Reads from the switches and saves it in KEY_Buffer along with number of bytes
			{chars_read += KEY_val;} // read the driver until EOF

		KEY_buffer[chars_read] ='\0'; // NULL terminate
		sscanf (KEY_buffer,"%d",&KEY_capture);//Converting the string input to integer for checking key presses
		chars_read = 0;


		if((KEY_capture==1)){//if first key is pressed, start the game!
			if(key_flag==0){
				key_flag=1;
				write(LED_FD,"1",strlen("1"));//lighting up the first LED for KEY0 press
			}
			else if(key_flag==1){
				key_flag=2;
				write(LED_FD,"0",strlen("0"));//swicthing off the LED and stopping the recording
				write (TIMER_FD,"end", (strlen("end"))+1);
				continue;

			}

		}

		if(key_flag==1){

			if (read (fd2, &ev, event_size) < event_size){
				continue;
			}
			//read (fd2, &ev, event_size);
			if (ev.type == EV_KEY && ev.value == KEY_PRESSED){
				printf("Pressed key: 0x%04x\n", (int)ev.code);
				if(i==0){
					write(TIMER_FD,"run",(strlen("run"))+1);
				}
				key_capture=(int)ev.code;
				no_keys+=1;

				while((TIMER_val=read(TIMER_FD,TIMER_buffer,BYTES))!=0){
					chars_read+=TIMER_val;}

				TIMER_buffer[chars_read]='\0';
				chars_read=0;
				sscanf(TIMER_buffer,"%02d%02d%02d",&curr_min,&curr_sec,&curr_microsec);

				press_record[i]=curr_min*10000+curr_sec*100+curr_microsec;

				if(no_keys>1){
					if(no_keys>2){
						co_eff[prev_press]=0;
					}
					prev_press=tone_index;
				}
				else prev_press=14;
				
				assign_coeff();
				tone_record[i]=key_capture;
				pthread_mutex_lock (&mutex_tone_volume);
				tone_volume[tone_index]=0xFD89D89;
				pthread_mutex_unlock (&mutex_tone_volume);
				i++;
			} 

			else if (ev.type == EV_KEY && ev.value == KEY_RELEASED){
				printf("Released key: 0x%04x\n", (int)ev.code);
				while((TIMER_val=read(TIMER_FD,TIMER_buffer,BYTES))!=0){
					chars_read+=TIMER_val;}

				TIMER_buffer[chars_read]='\0';
				chars_read=0;
				sscanf(TIMER_buffer,"%02d%02d%02d",&curr_min,&curr_sec,&curr_microsec);
				release_record[j]=curr_min*10000+curr_sec*100+curr_microsec;

				key_capture=(int)ev.code;
				assign_coeff();
				co_eff[tone_index]=0;
				j++;
				tone_index=prev_press;
				no_keys-=1;
				key_capture=0;
			}
		}

		if(KEY_capture==2){
			while((TIMER_val=read(TIMER_FD,TIMER_buffer,BYTES))!=0){
					chars_read+=TIMER_val;}

			TIMER_buffer[chars_read]='\0';
			chars_read=0;
			sscanf(TIMER_buffer,"%02d%02d%02d",&curr_min,&curr_sec,&curr_microsec);


			int final_time=curr_min*10000+curr_sec*100+curr_microsec;
			int elapsed_min,elapsed_sec,elapsed_microsec;


			elapsed_min=0;
			elapsed_sec=59-curr_sec;
			elapsed_microsec=99-curr_microsec;
			char elapsed_time[20];

			sprintf(elapsed_time,"%02d%02d%02d1",elapsed_min,elapsed_sec,elapsed_microsec);
			write(TIMER_FD,elapsed_time,(strlen(elapsed_time)+1));
			//printf("%s\n",elapsed_time);

			i=0;
			j=0;

			while((TIMER_val=read(TIMER_FD,TIMER_buffer,BYTES))!=0){
					chars_read+=TIMER_val;}

			TIMER_buffer[chars_read]='\0';
			chars_read=0;
			sscanf(TIMER_buffer,"%02d%02d%02d",&curr_min,&curr_sec,&curr_microsec);
			write(LED_FD,"2",strlen("2"));
			//write(TIMER_FD,"0100001",(strlen("0100001"))+1);


			int time1=curr_min*10000+curr_sec*100+curr_microsec;
			write(TIMER_FD,"run",(strlen("run"))+1);
			printf("%d,%d,%d\n",time1,press_record[0],final_time);
			while(time1){
				if(time1 == (press_record[i])-final_time){
					key_capture=tone_record[i];
					no_keys+=1;
					if(no_keys>1){
						if(no_keys>2){
							co_eff[prev_press]=0;
						}
						prev_press=tone_index;
					}
				else prev_press=14;
					assign_coeff();
					//tone_record[i]=key_capture;
					pthread_mutex_lock (&mutex_tone_volume);
					tone_volume[tone_index]=0xFD89D89;
					pthread_mutex_unlock (&mutex_tone_volume);
					i++;

				}
				else if(time1<=(release_record[j]-final_time)){
					key_capture=tone_record[j];
					assign_coeff();
					co_eff[tone_index]=0;
					j++;
					tone_index=prev_press;
					no_keys-=1;
					key_capture=0;
				}

				while((TIMER_val=read(TIMER_FD,TIMER_buffer,BYTES))!=0){ 
					chars_read+=TIMER_val;}

				TIMER_buffer[chars_read]='\0';

				chars_read=0;
			
				sscanf(TIMER_buffer,"%02d%02d%02d",&curr_min,&curr_sec,&curr_microsec);
				time1=curr_min*10000+curr_sec*100+curr_microsec;

				//printf("%d,%d,%d\n",time1,press_record[i],final_time);
			}

			write (TIMER_FD,"end", (strlen("end"))+1);

		}
		
	}//end of while loop.

	// Cancel the audio thread.
	pthread_cancel(tid_aud);

	// Wait until audio thread terminates.
	pthread_join(tid_aud, NULL);
	
	pthread_cancel(tid_vid);
	pthread_join(tid_vid, NULL);

	close(fd2);
	return 0;
}

//------------------------------------------------------------------------------------------------------------------------
void reset_tones(){

	int i=0;
	for(i=0;i<13;i++){
		tone_volume[i]=0xFD89D89;
	}
}
//------------------------------------------------------------------------------------------------------------------------
void assign_coeff(){
	if(key_capture==0x10){
		co_eff[0]=1;
		tone_index=0;
	}
	//else co_eff[0]=0;

	if(key_capture==0x03){
		co_eff[1]=1;
		tone_index=1;	
	}
	//else co_eff[1]=0;

	if(key_capture==0x11){
		co_eff[2]=1;
		tone_index=2;	
	}
	//else co_eff[2]=0;

	if(key_capture==0x04){
		co_eff[3]=1;
		tone_index=3;	
	}
	//else co_eff[3]=0;

	if(key_capture==0x12){
		co_eff[4]=1;
		tone_index=4;	
	}
	//else co_eff[4]=0;

	if(key_capture==0x13){
		co_eff[5]=1;
		tone_index=5;	
	}
	//else co_eff[5]=0;

	if(key_capture==0x06){
		co_eff[6]=1;
		tone_index=6;	
	}
	//else co_eff[6]=0;

	if(key_capture==0x14){
		co_eff[7]=1;
		tone_index=7;	
	}
	//else co_eff[7]=0;
	if(key_capture==0x07){
		co_eff[8]=1;
		tone_index=8;	
	}

	if(key_capture==0x15){
		co_eff[9]=1;
		tone_index=9;	
	}

	if(key_capture==0x08){
		co_eff[10]=1;
		tone_index=10;	
	}

	if(key_capture==0x16){
		co_eff[11]=1;
		tone_index=11;	
	}

	if(key_capture==0x17){
		co_eff[12]=1;
		tone_index=12;	
	}		

}

//------------------------------------------------------------------------------------------------------------------------
void clear_coeff(){
	int i=0;
	for(i=0;i<13;i++){
		co_eff[i]=0;
	}
}
//--------------------------------------------------------------------------------------------------------------------------

/* Open /dev/mem to give access to physical addresses */
int open_physical (int fd1)
{
	if (fd1 == -1) // check if already open
		if ((fd1 = open( "/dev/mem", (O_RDWR | O_SYNC))) == -1)
		{
			printf ("ERROR: could not open \"/dev/mem\"...\n");
			return (-1);
		}
	return fd1;
}
//--------------------------------------------------------------------------------------------------------------------------
/* Close /dev/mem to give access to physical addresses */
void close_physical (int fd1)
{
	close (fd1);
}

//---------------------------------------------------------------------------------------------------------------------------------------
/* Establish a virtual address mapping for the physical addresses starting
* at base and extending by span bytes */
void * map_physical(int fd1, unsigned int base, unsigned int span)
{
	void *virtual_base;
	// Get a mapping from physical addresses to virtual addresses
	virtual_base = mmap (NULL, span, (PROT_READ | PROT_WRITE), MAP_SHARED,fd1, base);
	
	if (virtual_base == MAP_FAILED)
	{
		printf ("ERROR: mmap() failed...\n");
		close (fd1);
		return (NULL);
	}
	return virtual_base;
}

/* Close the previously-opened virtual address mapping */
int unmap_physical(void * virtual_base, unsigned int span)
{
	if (munmap (virtual_base, span) != 0)
	{
		printf ("ERROR: munmap() failed...\n");
		return (-1);
	}
	return 0;
}

//--------------
void clear_screen(){
	
	sprintf(command,"clear ");//clearing
	write(VIDEO_FD,command,strlen(command));
}

void v_sync(){

	sprintf(command,"Vsync ");//synching
	write(VIDEO_FD,command,strlen(command));
}

void draw_line(int x1,int y1,int x2,int y2,int color){
	sprintf(command,"line %d,%d %d,%d %x",x1,y1,x2,y2,(color<<5));
	write(VIDEO_FD,command,strlen(command));
}
