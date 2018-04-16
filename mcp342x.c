#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>

#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define CONFIG_SIZE		5				
#define ADC_ADDR		0x68
#define I2C_DEVICE		"/dev/i2c-1"

#define CONFIG_MASK_READY		0x80
#define CONFIG_MASK_CHANNEL		0x60
#define CONFIG_MASK_CONV_MODE	0x10
#define CONFIG_MASK_SPS			0x0C
#define CONFIG_MASK_GAIN		0x03

struct mcp342x_config {
	uint8_t ready:1;
	uint8_t channel:2;
	uint8_t mode:1;
	uint8_t resolution:2;
	uint8_t gain:2;
	float lsb;
};

typedef enum {
	MODE_CONFIG,
	MODE_READ
} mode;

void printbincharpad(uint8_t c)
{
	for (int i = 7; i >= 0; --i)
	{
		putchar( (c & (1 << i)) ? '1' : '0' );
	}

	putchar('\n');
}

void mcp342x_read_config(int fd, struct mcp342x_config *config)
{
	uint8_t data[CONFIG_SIZE];
	int n;

	/* Read the chip's data */
	if((n = read(fd, data, sizeof(data))) < 0) {
		perror("Could not read data from ADC");
		exit(EXIT_FAILURE);
	}
	else {
		for(int i = 0; i < CONFIG_SIZE; i++)
			printbincharpad(data[i]);	
		printf("\n");
	}

	/* Find the config byte */
	uint8_t configbits;
	int bytepos = CONFIG_SIZE - 1;
	while(bytepos >= 3) {
		if(((1 << 7) | data[bytepos -1])  == data[bytepos]) {
			configbits = data[bytepos - 1];
			break;
		}
		bytepos--;
	}

	config->channel = ((configbits & CONFIG_MASK_CHANNEL) >> 5) + 1;
	config->mode = (configbits & CONFIG_MASK_CONV_MODE) >> 4;
	config->resolution = (configbits & CONFIG_MASK_SPS) >> 2;
	config->gain = (configbits & CONFIG_MASK_GAIN);
}

void mcp342x_print_config(struct mcp342x_config *config)
{
	/* ADC data ready? */
	uint8_t *rdystr = config->ready ? "Yes" : "No";

	/* Conversion mode? */
	char *convstr = config->mode ? "Continuous" : "One-shot";

	/* Sample rate? */
	uint8_t *spsstr;
	switch(config->resolution) {
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
	char gainstr[3];
	snprintf(gainstr, sizeof(gainstr), "x%i", config->gain + 1);
	
	printf("Ready: %s\n", rdystr);
	printf("Channel: %i\n", config->channel);
	printf("Conversion mode: %s\n", convstr);
	printf("Sample rate: %s\n", spsstr);
	printf("Gain: %s\n", gainstr);
}

int main(int argc, char **argv)
{
	mode mode;
	int configmodeopts = 0, readmodeopts = 0;
	struct mcp342x_config set_config = {};
	int ch;
	
	while((ch = getopt(argc, argv, "r:c")) != -1) {
		switch(ch) {
			case 'r':
				set_config.resolution = atoi(optarg);
				configmodeopts = 1;
				break;

			case 'c':
				readmodeopts = 1;
				break;
		}
	}

	/* The first non-option arg should be the mode */
	char *modearg = argv[optind];
	if(strcmp(modearg, "config") == 0) {
		mode = MODE_CONFIG;
	}
	else if(strcmp(modearg, "read") == 0) {
		mode = MODE_READ;
	}
	else {
		exit(EXIT_FAILURE);
	}

	/* Check for mode and args mismatches */
	if(((mode == MODE_CONFIG) && readmodeopts) || 
	   ((mode == MODE_READ) && configmodeopts)) {
	   	exit(EXIT_FAILURE);
	}
	   	
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

#if 0 
	uint8_t cfgbits = 0b00010000;
	if((n = write(i2cfd, &cfgbits, sizeof(cfgbits))) < 0) {
		perror("Failed writing configuration bits");
		exit(EXIT_FAILURE);
	}
#endif

	struct mcp342x_config config = {};
	mcp342x_read_config(i2cfd, &config);
	
	/* Print config if only arg is 'config' */
	if((mode == MODE_CONFIG) && !configmodeopts)
		mcp342x_print_config(&config);

/*
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
*/
	close(i2cfd);
}
