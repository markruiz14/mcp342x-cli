#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>

#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define CONFIG_SIZE		5				
#define ADC_ADDR 		0x68
#define I2C_DEVICE 		"/dev/i2c-1"

#define CONFIG_MASK_READY 		0x80
#define CONFIG_MASK_CHANNEL		0x60
#define CONFIG_MASK_CONV_MODE	0x10
#define CONFIG_MASK_SPS			0x0C
#define CONFIG_MASK_GAIN		0x03

void printbincharpad(uint8_t c)
{
	for (int i = 7; i >= 0; --i)
	{
		putchar( (c & (1 << i)) ? '1' : '0' );
	}

	putchar('\n');
}

int main(int argc, char **argv)
{
	int i2cfd;

	/* Open i2c device */
	if((i2cfd = open(I2C_DEVICE, O_RDWR)) < 0) {
		perror("Could not open i2c device");
		exit(EXIT_FAILURE);
	}

	/* Connect to the i2c device */ 
	if(ioctl(i2cfd, I2C_SLAVE, ADC_ADDR) < 0) {
		perror("Could not connect to ADC on i2c bus");
		exit(EXIT_FAILURE);
	}	

	int n;

#if 0 
	uint8_t cfgbits = 0b00010000;
	if((n = write(i2cfd, &cfgbits, sizeof(cfgbits))) < 0) {
		perror("Failed writing configuration bits");
		exit(EXIT_FAILURE);
	}
#endif

	uint8_t data[CONFIG_SIZE];

	/* Read the chip's data */
	if((n = read(i2cfd, data, sizeof(data))) < 0) {
		perror("Could not read data from ADC");
		exit(EXIT_FAILURE);
	}
	else {
		for(int i = 0; i < CONFIG_SIZE; i++)
			printbincharpad(data[i]);	
		printf("\n");
	}

	/* Find the config byte */
	uint8_t config;
	int bytepos = CONFIG_SIZE - 1;
	while(bytepos >= 3) {
		if(((1 << 7) | data[bytepos -1])  == data[bytepos]) {
			config = data[bytepos - 1];
			break;
		}
		bytepos--;
	}

	/* ADC data ready? */
	uint8_t *rdystr = (config & CONFIG_MASK_READY) ? "Yes" : "No";

	/* Selected channel? */
	uint8_t chan = (config & CONFIG_MASK_CHANNEL) >> 5;
		
	/* Conversion mode? */
	uint8_t *convstr = (config & CONFIG_MASK_CONV_MODE) ? "Continuous" : "One-shot";

	/* Sample rate? */
	uint8_t *spsstr;
	double lsb;
	int pga;
	switch((config & CONFIG_MASK_SPS) >> 2) {
		case 0:
			spsstr = "240 samples/sec (12 bits)";
			lsb = 0.001;
			break;
		case 1:
			spsstr = "60 samples/sec (14 bits)";
			lsb = 0.00025;
			break;
		case 2:
			spsstr = "15 samples/sec (16 bits)";
			lsb = 0.0000625;
			break;
		case 3:
			spsstr = "3.75 samples/sec (18 bits)";
			lsb = 0.000015625;
			break;
	}

	/* Gain? */
	char *gainstr;
	switch(config & CONFIG_MASK_GAIN) {
		case 0:
			gainstr = "x1";
			pga = 1;
			break;
		case 1:
			gainstr = "x2";
			pga = 2;
			break;
		case 2:
			gainstr = "x4";
			pga = 4;
			break;
		case 3:
			gainstr = "x8";
			pga = 8;
			break;
	}
	
	printf("Ready: %s\n", rdystr);
	printf("Channel: %i\n", chan + 1);
	printf("Conversion mode: %s\n", convstr);
	printf("Sample rate: %s\n", spsstr);
	printf("Gain: %s\n", gainstr);

	uint32_t outputcode;
	if((config & CONFIG_MASK_SPS) == 12) {
		outputcode = (data[0] << 16) | (data[1] << 8) | data[2];
	} 
	else {
		outputcode = (data[0] << 8) | data[1];
	}

	printf("outputcode: %i\n", outputcode);
			
	double value = outputcode * (lsb/pga);
	printf("value: %f\n", value);

	close(i2cfd);
}
