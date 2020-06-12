#include<stdio.h>
#include<signal.h>
#include<string.h>
#include<errno.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdlib.h>

#define BYTES 256
// max # of bytes to read from /dev/chardev
volatile sig_atomic_t stop;
void catchSIGINT(int signum){
		stop = 1;
		}

int main(int argc, char *argv[]){
		int KEY_FD,SW_FD,LED_FD,TIMER_FD,KEY_capture;
		int flag_strp=0;
		int flag_strq=0;
		int minutes,sec,microsec;
		int KEY_val,TIMER_val,SW_val,SW_capture,TIMER_capture,chars_read,Ref_Time;
		int Tot_time=0;
		
		// file descriptor
		
		char KEY_buffer[BYTES];
		char SW_buffer[BYTES];
		
		char TIMER_buffer[BYTES];
		char TIMER_ref[BYTES];//to capture the reference time set by the user
		int input1,input2,sum;//store questions and the answer
		int user_input;//to store the user's input
		int correct=0;//to store the number of correct answers
		
		
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
		if ((TIMER_FD = open("/dev/TIMER", O_RDWR)) == -1) 
				{
				printf("Error opening /dev/TIMER: %s\n", strerror(errno));
				return -1;
				}
		
		write (TIMER_FD,"end", (strlen("end"))+1);// stop the timer initially
		write(TIMER_FD,"5959991",(strlen("5959991"))+1);//writing the default time to the timer
		printf("Hello there!\n Welcome to this simple game of addition!!\n");
		printf("Please set the time using Keys:1,2,3 and switches. Press KEY0 when you are ready.\n");//get the time from the user
	

		while (!stop) {
				chars_read = 0;

				//Reading Key press
				while ((KEY_val = read (KEY_FD, KEY_buffer, BYTES)) != 0)//Reads from the switches and saves it in KEY_Buffer along with number of bytes
					chars_read += KEY_val; // read the driver until EOF

				KEY_buffer[chars_read] ='\0'; // NULL terminate

				sscanf (KEY_buffer,"%d",&KEY_capture);//Converting the string input to integer for checking key presses
				chars_read = 0;

				while ((SW_val = read (SW_FD, SW_buffer, BYTES)) != 0)//Reads from the switches and saves it in SW_Buffer along with number of bytes
						{chars_read += SW_val;}// read the driver until EOF
				SW_buffer[chars_read]='\0';//NULL terminates
				chars_read = 0;

				write(LED_FD,SW_buffer,strlen(SW_buffer));//LIGHTING UP THE LED W.R.T SW
				sscanf(SW_buffer,"%d",&SW_capture);//converting String from SW to integer to. Use this value to set time

				while ((TIMER_val = read (TIMER_FD, TIMER_buffer, BYTES)) != 0)//reads the current time
					chars_read += TIMER_val; // read the driver until EOF
				TIMER_buffer[chars_read]='\0';
				chars_read=0;

				//storing the current time read from the device driver
				sscanf(TIMER_buffer,"%d",&TIMER_capture);//reading the current time from the device
				microsec=TIMER_capture%100;
				TIMER_capture/=100;
				sec=TIMER_capture%100;
				TIMER_capture/=100;
				minutes=TIMER_capture;


				

				//Executes when a valid key is pressed 
				if(KEY_capture==1)//if first key is pressed, start the game!
						{
							while ((TIMER_val = read (TIMER_FD, TIMER_buffer, BYTES)) != 0)////reading the time, which would have been set by the user
								chars_read += TIMER_val; // read the driver until EOF
							TIMER_buffer[chars_read]='\0';//time set by user 
							chars_read=0;

							write(TIMER_FD,"run",(strlen("run"))+1);//start the timer
							
							strcpy(TIMER_ref,TIMER_buffer);//get the reference time
							strcat(TIMER_ref,"1");//1 is concatented for our internal logic(Button varible in kernel module code)

							input1=rand()%50;//randomizing the questions
							input2=rand()%50;
							printf("%d + %d =",input1,input2);// the questions are printed on to the kernel
							sum=input1+input2;
							flag_strp=1;//flag set to indiacte that game has started
						}


				else if (KEY_capture==2)//if the second key is pressed
						{
							
							sprintf(SW_buffer,"%02d%02d%02d%d",minutes,sec,SW_capture,1);
							
							write(TIMER_FD,SW_buffer,(strlen(SW_buffer))+1);//writing the time chaged by the user to the drive
						}

				else if (KEY_capture==4)//if the third key is pressed
						{
							
							sprintf(SW_buffer,"%02d%02d%02d%d",minutes,SW_capture,microsec,2);
							
							write(TIMER_FD,SW_buffer,(strlen(SW_buffer))+1);//writing the time chaged by the user to the drive
						}
				else if (KEY_capture==8)//if the fourth key is pressed
						{
							
							sprintf(SW_buffer,"%02d%02d%02d%d",SW_capture,sec,microsec,3);
							
							write(TIMER_FD,SW_buffer,(strlen(SW_buffer))+1);//writing the time chaged by the user to the drive
						}
					
					KEY_capture=0;

				while(flag_strp==1)//if the game has started, flag_strp=1.
					{
							scanf("%d",&user_input);//wait for user input
						
							while ((TIMER_val = read (TIMER_FD, TIMER_buffer, BYTES)) != 0)
									{chars_read += TIMER_val;} // read the driver until EOF
							TIMER_buffer[chars_read]='\0';
							chars_read=0;//the current time is read from the TIMER
							usleep(100);

							sscanf(TIMER_buffer,"%d",&TIMER_capture);//changing the time to int for comparison
							sscanf(TIMER_ref,"%d",&Ref_Time);
							//TIMER_avg=((TIMER_avg/10000)*60)+(TIMER_avg%100);
							//printf("TIme captured:%d\n",TIMER_capture);

							if(TIMER_capture==0)//If Timeout!
								{
									flag_strp=0;
									flag_strq=1;
									break;//break out of the loop

								}
						
							else// if not timeout
							{
								if(sum==user_input)//if correct answer
									{
										printf("Correct!\n");
										correct=correct+1;
										//printf("Ref_Time1:%d\n",Ref_Time);
										Ref_Time=((Ref_Time/100000)*60)+((Ref_Time/1000)%100);
										//printf("Ref_Time2:%d\n",Ref_Time);
										//printf("TIME_capture1:%d\n",TIMER_capture);
										TIMER_capture=((TIMER_capture/10000)*60)+((TIMER_capture/100)%100);
										//printf("TIME_capture2:%d\n",TIMER_capture);
										Tot_time+=(Ref_Time-TIMER_capture);
										write(TIMER_FD,TIMER_ref,(strlen(TIMER_ref))+1);
									}

								else
									{printf("Try Again!\n");
										continue;}

								//Ask new questions
								input1=rand()%(50+correct*15)+correct*7;
								input2=rand()%(50+correct*10);
								printf("%d + %d =",input1,input2);
								sum=input1+input2;
							}

					}

					if(flag_strq==1)//if timer has expired, flag_strq=1
						{
							printf("Oops! Time expired!! You answered %d questions correctly in time %d.\n",correct,Tot_time);
							if(correct!=0)
								{
									printf("\nThe average number amount of time you took to answer a question:%d\n",Tot_time/correct);}
							else printf("You answered 0 questions\n");

							usleep(100);
							break;
						}
				usleep(100);
				}//while loop end
				
		// closing all the files
		close (KEY_FD);
		close (SW_FD);
		close (LED_FD);
		close (TIMER_FD);
		return 0;
		}