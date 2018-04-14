#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>

#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define CONFIG_SIZE		4
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

	unsigned long funcs;
	if(ioctl(i2cfd, I2C_FUNCS, &funcs) < 0) {
		perror("Failed to get i2c device functionality");
		exit(EXIT_FAILURE);
	}

	uint8_t data[CONFIG_SIZE];
	int n;

	/* Read the chip's data (3 bytes) */
	if((n = read(i2cfd, data, sizeof(data))) < 0) {
		perror("Could not read data from ADC");
		exit(EXIT_FAILURE);
	}
	else {
		for(int i = 0; i < CONFIG_SIZE; i++)
			printbincharpad(data[i]);	
	}

	uint8_t config = data[2];
	
	/* ADC data ready? */
	uint8_t *rdystr = (config & CONFIG_MASK_READY) ? "Yes" : "No";

	/* Selected channel? */
	uint8_t chan = config & CONFIG_MASK_CHANNEL;
	
	/* Conversion mode? */
	uint8_t *convstr = (config & CONFIG_MASK_CONV_MODE) ? "Continuous" : "One-shot";

	/* Sample rate? */
	uint8_t *spsstr;
	switch(config & CONFIG_MASK_SPS) {
		case 0:
			spsstr = "240 samples/sec (12 bits)";
			break;
		case 1:
			spsstr = "60 samples/sec (14 bits)";
			break;
		case 2:
			spsstr = "15 samples/sec (16 bits)";
			break;
		case 3:
			spsstr = "3.75 samples/sec (18 bits)";
			break;
	}

	/* Gain? */
	char *gainstr;
	switch(config & CONFIG_MASK_GAIN) {
		case 0:
			gainstr = "x1";
			break;
		case 1:
			gainstr = "x2";
			break;
		case 2:
			gainstr = "x4";
			break;
		case 3:
			gainstr = "x8";
			break;
	}
	
	printf("Ready: %s\n", rdystr);
	printf("Channel: %i\n", chan + 1);
	printf("Conversion mode: %s\n", convstr);
	printf("Sample rate: %s\n", spsstr);
	printf("Gain: %s\n", gainstr);

	double lsb = 0.001;
	int pga = 1;

	uint16_t outputCode = (data[0] << 8 ) | data[1];
	printf("outputCode: %i\n", outputCode);
	
	double value = outputCode * (lsb/pga);
	printf("value: %f\n", value);

	close(i2cfd);
}
