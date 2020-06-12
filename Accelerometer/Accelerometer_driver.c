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
#include "ADXL345.h"

/* Kernel character device driver /dev/LED. */
static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);

static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

// ADXL345 Functions
void ADXL345_Init(void);
void ADXL345_Calibrate(void);
bool ADXL345_IsDataReady(void);
bool ADXL345_WasActivityUpdated(void);
void ADXL345_XYZ_Read(int16_t szData16[3]);
void ADXL345_IdRead(uint8_t *pId);
void ADXL345_REG_READ(uint8_t address, uint8_t *value);
void ADXL345_REG_WRITE(uint8_t address, uint8_t value);
void ADXL345_REG_MULTI_READ(uint8_t address, uint8_t values[], uint8_t len);

bool ADXL345_SingleTap(void);
bool ADXL345_DoubleTap(void);

//void tap_init(void);
int device_id_check(void);
void read_accel(void);
// I2C0 Functions
void I2C0_Init(void);
//void I2C0_Enable_FPGA_Access(void);

// Pinmux Functions
void Pinmux_Config(void);


//--------------------------------------------------------------------------------
//FOPS for the different devices

static struct file_operations ACCEL_fops = {
        .owner = THIS_MODULE,
        .read = device_read,
        .write = device_write,
        .open = device_open,
        .release = device_release
        };

//Macros
#define SUCCESS 0
#define DEV1_NAME "ACCEL"
#define MAX_SIZE 20+1


static struct miscdevice ACCEL = {
        .minor = MISC_DYNAMIC_MINOR,
        .name = DEV1_NAME,
        .fops = &ACCEL_fops,
        .mode = 0666
        };      

static int ACCEL_registered = 0;

static char CMD_value[MAX_SIZE];

static char accel_msg[MAX_SIZE];

volatile int *accel_ptr;

volatile int *GMUX_Virtual;       //physical addresses for LW bridge
volatile int *I2C_Virtual;

int accel_capture;
int mg_per_lsb=31;

static int __init init_ACCEL(void)
{

    uint8_t devid;
    int err;
    //int16_t mg_per_lsb = 31;
    
    GMUX_Virtual = ioremap_nocache(SYSMGR_BASE,SYSMGR_SPAN);
    I2C_Virtual  = ioremap_nocache(I2C0_BASE,I2C0_SPAN);

    err = misc_register (&ACCEL);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV1_NAME);
        return err;//returns if device registration fails
        }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", DEV1_NAME);
        ACCEL_registered=1;
        }

    Pinmux_Config();
    I2C0_Init();

    if (device_id_check()){
    
        ADXL345_Init();
    	//tap_init();
    }
    else {
        printk (KERN_ERR "Error: I2C communication failed\n");
    return -1;
    }
    
     return err;
}
//----------------------------------------------------------------------------------------------------
static void __exit stop_ACCEL(void) {
    if (ACCEL_registered) {
        misc_deregister (&ACCEL);

        iounmap (GMUX_Virtual);
        iounmap (I2C_Virtual);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV1_NAME);

    }

}
//----------------------------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------------------------
static ssize_t device_read(struct file *filp, char *buffer, size_t length,
loff_t *offset)
        {
        //ASSIGN VALUE TO ACCEL MSG, ie, read from ADXL
        


        size_t bytes;
        
        //static char *accel_msg;
        // R=0;
        // if (ADXL345_WasActivityUpdated())
        //     {R=1;}
        // else {R=0;}

        // ADXL345_XYZ_Read(XYZ);

        // sprintf(accel_msg,"%d X=%d mg, Y=%d mg, Z=%d 31",R, XYZ[0], XYZ[1], XYZ[2]);
        read_accel();        
        bytes = strlen (accel_msg) - (*offset);
        // how many bytes not yet sent?
        bytes = bytes > length ? length : bytes;
        // too much to send at once?
        if (bytes)
        (void) copy_to_user (buffer, &accel_msg[*offset], bytes);//sending the time read to the user
        // keep track of number of bytes sent to the user
        *offset = bytes;
        return bytes;
        }
//----------------------------------------------------------------------------------------------------
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
        {
        size_t bytes;
        uint8_t devid;
        int R,F,G;
        bytes = length;
        if (bytes > MAX_SIZE - 1)
        bytes = MAX_SIZE - 1;
        (void) copy_from_user (CMD_value, buffer, bytes);
        CMD_value[bytes-1] ='\0'; // NULL terminate

        if(strcmp(CMD_value,"device")==0){
            ADXL345_REG_READ(0x00, &devid);
            printk(KERN_ERR "Device ID:%x\n",devid);
        }


        if(strcmp(CMD_value,"init")==0){
            ADXL345_Init();
            printk(KERN_ERR "Device Initialized!\n");

        }

        if(strcmp(CMD_value,"cali")==0){
            ADXL345_Calibrate();
            printk(KERN_ERR "Device Calibrated!\n");
        }

        if(sscanf(CMD_value,"format %d %d",&F,&G)){
            // +- 16g range, 10 bit resolution
            if(F==0){

                if(G==2){
                ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, (XL345_RANGE_2G | XL345_10BIT));
                    }

                if(G==4){
                ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, (XL345_RANGE_4G | XL345_10BIT));
                    }

                if(G==8){
                ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, (XL345_RANGE_8G | XL345_10BIT));
                    }

                if(G==16){
                ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, (XL345_RANGE_16G | XL345_10BIT));
                    }              
                }

            else if(F==1){
                ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, (XL345_RANGE_16G | XL345_FULL_RESOLUTION));

                    if(G==2){
                        ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, (XL345_RANGE_2G | XL345_FULL_RESOLUTION));
                    }

                if(G==4){
                        ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, (XL345_RANGE_4G | XL345_FULL_RESOLUTION));
                    }

                if(G==8){
                        ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, (XL345_RANGE_8G | XL345_FULL_RESOLUTION));
                    }

                if(G==16){
                        ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, (XL345_RANGE_16G | XL345_FULL_RESOLUTION));
                    }  
                }
    
         }

         // Output Data Rate: 
        if(sscanf(CMD_value,"rate %d",&R)){
            // Output Data Rate: 
            if(R==12){
                ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_12_5);
            }

            if(R==25){
                ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_25);
            }
            
        }
        
        return bytes;
        }
//----------------------------------------------------------------------------------------------------
bool ADXL345_SingleTap(){
    bool bReady = false;
    uint8_t data8;
    
    ADXL345_REG_READ(ADXL345_REG_INT_SOURCE,&data8);
    if (data8 & XL345_SINGLETAP)
        bReady = true;
    
    return bReady;
}

bool ADXL345_DoubleTap(){
    bool bReady = false;
    uint8_t data8;
    
    ADXL345_REG_READ(ADXL345_REG_INT_SOURCE,&data8);
    if (data8 & XL345_DOUBLETAP)
        bReady = true;
    
    return bReady;
}



void Pinmux_Config(){
    // Set up pin muxing (in sysmgr) to connect ADXL345 wires to I2C0
    *(GMUX_Virtual+SYSMGR_I2C0USEFPGA) = 0;
    *(GMUX_Virtual+SYSMGR_GENERALIO7) = 1;
    *(GMUX_Virtual+SYSMGR_GENERALIO8) = 1;
}

// Initialize the I2C0 controller for use with the ADXL345 chip
void I2C0_Init(){

    // Abort any ongoing transmits and disable I2C0.
    *(I2C_Virtual + I2C0_ENABLE) = 2;
    
    // Wait until I2C0 is disabled
    while(((*(I2C_Virtual +I2C0_ENABLE_STATUS))&0x1) == 1){}
    
    // Configure the config reg with the desired setting (act as 
    // a master, use 7bit addressing, fast mode (400kb/s)).
    *(I2C_Virtual +I2C0_CON) = 0x65;
    
    // Set target address (disable special commands, use 7bit addressing)
    *(I2C_Virtual +I2C0_TAR) = 0x53;
    
    // Set SCL high/low counts (Assuming default 100MHZ clock input to I2C0 Controller).
    // The minimum SCL high period is 0.6us, and the minimum SCL low period is 1.3us,
    // However, the combined period must be 2.5us or greater, so add 0.3us to each.
    *(I2C_Virtual +I2C0_FS_SCL_HCNT) = 60 + 30; // 0.6us + 0.3us
    *(I2C_Virtual +I2C0_FS_SCL_LCNT) = 130 + 30; // 1.3us + 0.3us
    
    // Enable the controller
    *(I2C_Virtual +I2C0_ENABLE) = 1;
    
    // Wait until controller is enabled
    while((  (*(I2C_Virtual +I2C0_ENABLE_STATUS))   &0x1) == 0){}
    
}

// Function to allow components on the FPGA side (ex. Nios II processors) to 
// access the I2C0 controller through the F2H bridge. This function
// needs to be called from an ARM program, and to allow a Nios II program
// to access the I2C0 controller.
/*
void I2C0_Enable_FPGA_Access(){

    // Deassert fpga bridge resets
    *RSTMGR_BRGMODRST = 0;
    
    // Enable non-secure masters to access I2C0
    *L3REGS_L4SP = *L3REGS_L4SP | 0x4;
    
    // Enable non-secure masters to access pinmuxing registers (in sysmgr)
    *L3REGS_L4OSC1 = *L3REGS_L4OSC1 | 0x10;
}
*/
// Write value to internal register at address
void ADXL345_REG_WRITE(uint8_t address, uint8_t value){
    
    // Send reg address (+0x400 to send START signal)
    *(I2C_Virtual +I2C0_DATA_CMD) = address + 0x400;
    
    // Send value
    *(I2C_Virtual +I2C0_DATA_CMD) = value;
}

// Read value from internal register at address
void ADXL345_REG_READ(uint8_t address, uint8_t *value){

    // Send reg address (+0x400 to send START signal)
    *(I2C_Virtual +I2C0_DATA_CMD) = address + 0x400;
    
    // Send read signal
    *(I2C_Virtual +I2C0_DATA_CMD) = 0x100;
    
    // Read the response (first wait until RX buffer contains data)  
    while (*(I2C_Virtual +I2C0_RXFLR) == 0){}
    *value = *(I2C_Virtual +I2C0_DATA_CMD);
}

// Read multiple consecutive internal registers
void ADXL345_REG_MULTI_READ(uint8_t address, uint8_t values[], uint8_t len){

    int i;
    int nth_byte=0;
    // Send reg address (+0x400 to send START signal)
    *(I2C_Virtual +I2C0_DATA_CMD) = address + 0x400;
    
    // Send read signal len times

    for (i=0;i<len;i++)
        *(I2C_Virtual +I2C0_DATA_CMD) = 0x100;

    // Read the bytes
    
    while (len){
        if ( (*(I2C_Virtual +I2C0_RXFLR)) > 0){
            values[nth_byte] = *(I2C_Virtual +I2C0_DATA_CMD);
            nth_byte++;
            len--;
        }
    }
}

// Initialize the ADXL345 chip
void ADXL345_Init(){

    // +- 12_5g range, 10 bit resolution
    ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, XL345_RANGE_16G | XL345_10BIT);
    
    // Output Data Rate: 12Hz
    ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_12_5);

    // NOTE: The DATA_READY bit is not reliable. It is updated at a much higher rate than the Data Rate
    // Use the Activity and Inactivity interrupts.
    //----- Enabling interrupts -----//
    ADXL345_REG_WRITE(ADXL345_REG_THRESH_ACT, 0x04);    //activity threshold
    ADXL345_REG_WRITE(ADXL345_REG_THRESH_INACT, 0x02);  //inactivity threshold
    ADXL345_REG_WRITE(ADXL345_REG_TIME_INACT, 0x02);    //time for inactivity
    ADXL345_REG_WRITE(ADXL345_REG_ACT_INACT_CTL, 0xFF); //Enables AC coupling for thresholds
    ADXL345_REG_WRITE(ADXL345_REG_INT_ENABLE, XL345_ACTIVITY | XL345_INACTIVITY | XL345_DOUBLETAP | XL345_SINGLETAP);  //enable interrupts
    //-------------------------------//
    ADXL345_REG_WRITE(0x1D,0x30);//threshold 48---->3g
    ADXL345_REG_WRITE(0x22,0x10);//latent 5
    ADXL345_REG_WRITE(0x23,0xF0);//window 255
    ADXL345_REG_WRITE(0x21,0x20);//duration 0.02
    ADXL345_REG_WRITE(0x2A,0x01);//enable tap on z axis
    // stop measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_STANDBY);
    
    // start measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_MEASURE);
}

// Calibrate the ADXL345. The DE1-SoC should be placed on a flat
// surface, and must remain stationary for the duration of the calibration.
void ADXL345_Calibrate(){
    
    int average_x = 0;
    int average_y = 0;
    int average_z = 0;
    int16_t XYZ[3];
    int8_t offset_x;
    int8_t offset_y;
    int8_t offset_z;
    uint8_t saved_bw;
    uint8_t saved_dataformat;
    int i = 0;
    
    // stop measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_STANDBY);
    
    // Get current offsets
    ADXL345_REG_READ(ADXL345_REG_OFSX, (uint8_t *)&offset_x);
    ADXL345_REG_READ(ADXL345_REG_OFSY, (uint8_t *)&offset_y);
    ADXL345_REG_READ(ADXL345_REG_OFSZ, (uint8_t *)&offset_z);
    
    // Use 100 hz rate for calibration. Save the current rate.
    
    ADXL345_REG_READ(ADXL345_REG_BW_RATE, &saved_bw);
    ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_100);
    
    // Use 16g range, full resolution. Save the current format.
    
    ADXL345_REG_READ(ADXL345_REG_DATA_FORMAT, &saved_dataformat);
    ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, XL345_RANGE_16G | XL345_FULL_RESOLUTION);
    
    // start measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_MEASURE);
    
    // Get the average x,y,z accelerations over 32 samples (LSB 3.9 mg)
    
    while (i < 32){
        // Note: use DATA_READY here, can't use ACTIVITY because board is stationary.
        if (ADXL345_IsDataReady()){
            ADXL345_XYZ_Read(XYZ);
            average_x += XYZ[0];
            average_y += XYZ[1];
            average_z += XYZ[2];
            i++;
        }
    }
    average_x = ROUNDED_DIVISION(average_x, 32);
    average_y = ROUNDED_DIVISION(average_y, 32);
    average_z = ROUNDED_DIVISION(average_z, 32);
    
    // stop measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_STANDBY);
    
    // printf("Average X=%d, Y=%d, Z=%d\n", average_x, average_y, average_z);
    
    // Calculate the offsets (LSB 15.6 mg)
    offset_x += ROUNDED_DIVISION(0-average_x, 4);
    offset_y += ROUNDED_DIVISION(0-average_y, 4);
    offset_z += ROUNDED_DIVISION(256-average_z, 4);
    
    // printf("Calibration: offset_x: %d, offset_y: %d, offset_z: %d (LSB: 15.6 mg)\n",offset_x,offset_y,offset_z);
    
    // Set the offset registers
    ADXL345_REG_WRITE(ADXL345_REG_OFSX, offset_x);
    ADXL345_REG_WRITE(ADXL345_REG_OFSY, offset_y);
    ADXL345_REG_WRITE(ADXL345_REG_OFSZ, offset_z);
    
    // Restore original bw rate
    ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, saved_bw);
    
    // Restore original data format
    ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, saved_dataformat);
    
    // start measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_MEASURE);
}

// Return true if there was activity since the last read (checks ACTIVITY bit).
bool ADXL345_WasActivityUpdated(){
    bool bReady = false;
    uint8_t data8;
    
    ADXL345_REG_READ(ADXL345_REG_INT_SOURCE,&data8);
    if (data8 & XL345_ACTIVITY)
        bReady = true;
    
    return bReady;
}

// Return true if there is new data (checks DATA_READY bit).
bool ADXL345_IsDataReady(){
    bool bReady = false;
    uint8_t data8;
    
    ADXL345_REG_READ(ADXL345_REG_INT_SOURCE,&data8);
    if (data8 & XL345_DATAREADY)
        bReady = true;
    
    return bReady;
}

// Read acceleration data of all three axes
void ADXL345_XYZ_Read(int16_t szData16[3]){
    uint8_t szData8[6];
    ADXL345_REG_MULTI_READ(0x32, (uint8_t *)&szData8, sizeof(szData8));

    szData16[0] = (szData8[1] << 8) | szData8[0]; 
    szData16[1] = (szData8[3] << 8) | szData8[2];
    szData16[2] = (szData8[5] << 8) | szData8[4];
}

// Read the ID register
void ADXL345_IdRead(uint8_t *pId){
    ADXL345_REG_READ(ADXL345_REG_DEVID, pId);
}
//-------------------------------------------------------------------------------
int device_id_check(void)
{
    uint8_t devid;
    // 0xE5 is read from DEVID(0x00) if I2C is functioning correctly
    ADXL345_REG_READ(0x00, &devid);
    
    // Correct Device ID
    if (devid == 0xE5)
        return 1;
    return 0;
    
}
//-------------------------------------------------------------------------------------
void read_accel(void)
{
    int16_t XYZ[3];
    //dmesint isReady = ADXL345_IsDataReady();
    uint8_t intrp_source_reg;
        
    if (device_id_check()){       
        ADXL345_XYZ_Read(XYZ);
        ADXL345_REG_READ(ADXL345_REG_INT_SOURCE,&intrp_source_reg);
        //sprintf(accel_msg, "%d %d %d %04d %04d %04d %d\n", ADXL345_SingleTap(),ADXL345_DoubleTap(),ADXL345_WasActivityUpdated(),XYZ[0], XYZ[1], XYZ[2], mg_per_lsb);
        //sprintf(accel_msg, "%d %d %d %04d %04d %04d %d\n", ADXL345_SingleTap(),ADXL345_DoubleTap(),ADXL345_WasActivityUpdated(),XYZ[0], XYZ[1], XYZ[2], mg_per_lsb);
        sprintf(accel_msg,"%02x %04d %04d %04d %d", intrp_source_reg, XYZ[0], XYZ[1], XYZ[2], mg_per_lsb);
    }
}
MODULE_LICENSE("GPL");
module_init (init_ACCEL);
module_exit (stop_ACCEL);