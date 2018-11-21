#include <stdio.h>
#include <pigpio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <memory.h>

#define SDA 2 // Serial Data (GPIO18)
#define SCL 3 // Serial Clock (GPIO19)
#define RPI_ADDR 0x13 // Slave address

bsc_xfer_t xfer;

//######################################################################
// VERY IMPORTANT LINKS!!
// https://github.com/joan2937/pigpio/blob/master/pigpio.h
// http://abyz.me.uk/rpi/pigpio/cif.html
//######################################################################

void I2C_Comm(int event, uint32_t tick)
{
	if(xfer.rxCnt != 0)
	{
			printf("Received %d bytes\n", xfer.rxCnt); // 1 char = 1 byte

			for(int j=0;j < xfer.rxCnt;j++) // Print bytes received in rxBuf
				printf("%c",xfer.rxBuf[j]);	
	}
	else 	printf("Received nothing!!\n");
	
}

int main(int argc, char *argv[]){
	
	// Set BSC mode -> xfer.control = (addr<<16) | "check page for flags"
	// 0x305 -> EN, I2, ET, RE
	xfer.control = (RPI_ADDR<<16) | 0x305; //Init I2C comm
	
	if (gpioInitialise() < 0) {printf("Error 1\n"); return 1;}
	
	// Enable pull-ups
	gpioSetPullUpDown(SDA,PI_PUD_UP);
	gpioSetPullUpDown(SCL,PI_PUD_UP);
	
	// Callback function: run when message is sent from Arduino to RPI
	eventSetFunc(PI_EVENT_BSC,I2C_Comm);

    while(key = getchar() != 'q'){
        
        if (xfer.rxCnt != 0)
        {
            printf("Received %d bytes\n", xfer.rxCnt); // 1 char = 1 byte

            for (int j = 0; j < xfer.rxCnt; j++) // Print bytes received in rxBuf
                printf("%c", xfer.rxBuf[j]);
        }
    }
		
	return 0;
}
