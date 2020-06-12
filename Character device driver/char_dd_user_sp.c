#include<stdio.h>
#include<signal.h>
#include<string.h>
#include<errno.h>
#include<fcntl.h>
#include<unistd.h>


#define BYTES 256
// max # of bytes to read from /dev/chardev
volatile sig_atomic_t stop;
void catchSIGINT(int signum){
		stop = 1;
		}
/* This code uses the character device driver /dev/chardev. The code reads the
* default message from the driver and then prints it. After this the code
* changes the message in a loop by writing to the driver, and prints each new
* message. The program exits if it receives a kill signal (for example, ^C
* typed on stdin). */
int main(int argc, char *argv[]){
		int KEY_FD,SW_FD,LED_FD,HEX_FD,KEY_flag;
		// file descriptor
		//char chardev_buffer[BYTES]; // buffer for chardev character data
		char KEY_buffer[BYTES];
		char SW_buffer[BYTES];
		//char LED_buffer[BYTES];
		//char HEX_buffer[BYTES];

		int KEY_val,SW_val,SW_capture,SW_acc=0,chars_read;
		// number of characters read
		
		// catch SIGINT from ^C, instead of having it abruptly close this program
		signal(SIGINT, catchSIGINT);
		// Open the character device driver for read/write
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

		if ((LED_FD = open("/dev/LED", O_RDWR)) == -1) 
				{
				printf("Error opening /dev/LED: %s\n", strerror(errno));
				return -1;
				}
		if ((HEX_FD = open("/dev/HEX", O_RDWR)) == -1) 
				{
				printf("Error opening /dev/HEX: %s\n", strerror(errno));
				return -1;
				}
		//*(SW_buffer)="0";
		write (HEX_FD, "000000\0", strlen("000000\0"));

		while (!stop) {
				chars_read = 0;

				//Reading
				while ((KEY_val = read (KEY_FD, KEY_buffer, BYTES)) != 0)//Reads from the switches and saves it in KEY_Buffer along with number of bytes
					chars_read += KEY_val; // read the driver until EOF

				KEY_buffer[chars_read] ='\0'; // NULL terminate
				sscanf (KEY_buffer,"%d",&KEY_flag);//Converting the string input to integer for checking valid key press
				chars_read = 0;

				while ((SW_val = read (SW_FD, SW_buffer, BYTES)) != 0)//Reads from the switches and saves it in SW_Buffer along with number of bytes
						{chars_read += SW_val;}// read the driver until EOF
				SW_buffer[chars_read]='\0';
				chars_read = 0;//NULL terminates

				//Executes when a valid key is pressed 
				if(KEY_flag)	{
					printf("%s,\n", SW_buffer);
					write (LED_FD, SW_buffer, strlen(SW_buffer));//writes the value in SW_buffer to LED_FD which is given to the LED drivers

					sscanf(SW_buffer,"%d",&SW_capture);//accumulating the values from switch in SW_capture
					SW_acc=SW_acc+SW_capture;
					sprintf(SW_buffer,"%d",SW_acc);
					write (HEX_FD, SW_buffer, strlen(SW_buffer));//writes the value in SW_buffer to HEX_FD which is given to the HEX drivers
					KEY_flag=0;
					}
				usleep(100);
				}
		// closing all the files
		close (KEY_FD);
		close (SW_FD);
		close (LED_FD);
		close (HEX_FD);
		return 0;
		}