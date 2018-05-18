/*
 * mcp342x
 *
 * A command line tool for configuring and reading data from the MCP342x family 
 * of 18-Bit, multichannel ADC chips with I2C interface.
 *
 * Copyright (C) 2017 Mark Ruiz (mark@markruiz.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <errno.h>

#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define CONFIG_SIZE		5				
#define DBG_PRINT_READ_BITS	0

#define CONFIG_MASK_READY	0x80
#define CONFIG_MASK_CHANNEL	0x60
#define CONFIG_MASK_CONV_MODE	0x10
#define CONFIG_MASK_SPS		0x0C
#define CONFIG_MASK_GAIN	0x03

#define CONFIG_RES_12BITS	0x00
#define CONFIG_RES_14BITS	0x01
#define CONFIG_RES_16BITS	0x02
#define CONFIG_RES_18BITS	0x03

#define GEN_CALL_ADDR		0x00
#define GEN_CALL_CMD_RESET	0x06
#define GEN_CALL_CMD_LATCH	0x04
#define GEN_CALL_CMD_CONV	0x08

#define get_msb(n, bits) ((1 << (bits - 1)) & n)

struct mcp342x_config {
	uint8_t ready;
	uint8_t channel;
	uint8_t mode;
	uint8_t resolution;
	uint8_t gain;
	uint32_t outputcode;
	float lsb;
};

typedef enum {
	MODE_CONFIG,
	MODE_READ,
	MODE_RESET
} mode;

void printbincharpad(uint8_t c)
{
	for (int i = 7; i >= 0; --i)
		putchar( (c & (1 << i)) ? '1' : '0' );

	putchar('\n');
}

void mcp342x_print_config(struct mcp342x_config config)
{
	/* ADC data ready? */
	uint8_t *rdystr = config.ready ? "Yes" : "No";

	/* Conversion mode? */
	char *convstr = config.mode ? "Continuous" : "One-shot";

	/* Sample rate? */
	uint8_t *spsstr;
	switch (config.resolution) {
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
	snprintf(gainstr, sizeof(gainstr), "x%i", config.gain);
	
	printf("Ready: %s\n", rdystr);
	printf("Channel: %i\n", config.channel);
	printf("Conversion mode: %s\n", convstr);
	printf("Sample rate: %s\n", spsstr);
	printf("Gain: %s\n", gainstr);
}

int mcp342x_read_config(int fd, struct mcp342x_config *config)
{
	uint8_t data[CONFIG_SIZE];
	int n;

	/* Read the chip's data */
	if ((n = read(fd, data, sizeof(data))) < 0) 
		return -1;

#if DBG_PRINT_READ_BITS == 1
	for (int i = 0; i < CONFIG_SIZE; i++)
		printbincharpad(data[i]);	
	printf("\n");
#endif

	/* Find the config byte */
	uint8_t configbits;
	int bytepos = CONFIG_SIZE - 1;
	while (bytepos >= 3) {
		if (((1 << 7) | data[bytepos -1])  == data[bytepos]) {
			configbits = data[bytepos - 1];
			break;
		}
		bytepos--;
	}

	config->channel = ((configbits & CONFIG_MASK_CHANNEL) >> 5) + 1;
	config->mode = (configbits & CONFIG_MASK_CONV_MODE) >> 4;
	config->resolution = (configbits & CONFIG_MASK_SPS) >> 2;
	config->gain = (configbits & CONFIG_MASK_GAIN) + 1;

	/* LSB is mapped to resolution */
	switch (config->resolution) {
	case 0:
		config->lsb = 0.001;
		break;
	case 1:
		config->lsb = 0.00025;
		break;
	case 2:
		config->lsb = 0.0000625;
		break;
	case 3:
		config->lsb = 0.000015625;
		break;
	}

	/* Save the output code */
	if (config->resolution == CONFIG_RES_18BITS) 
		config->outputcode = (data[0] << 16) | (data[1] << 8) | data[2];
	else 
		config->outputcode = (data[0] << 8) | data[1];

	return 0;
}

void mcp342x_apply_config(struct mcp342x_config src, struct mcp342x_config *dest)
{
	if (src.ready != 0xFF)
		dest->ready = src.ready;

	if (src.channel != 0xFF)
		dest->channel = src.channel;

	if (src.mode != 0xFF)
		dest->mode = src.mode;

	if (src.resolution != 0xFF)
		dest->resolution = src.resolution;

	if (src.gain != 0xFF)
		dest->gain= src.gain;
}

int mcp342x_write_config(int fd, struct mcp342x_config *config)
{
	uint8_t byte = 0;

	byte |= config->ready << 7;
	byte |= (config->channel - 1) << 5;
	byte |= config->mode << 4;
	byte |= config->resolution << 2;
	byte |= config->gain - 1;

	return write(fd, &byte, sizeof(byte));
}

float mcp342x_get_value(int fd, struct mcp342x_config *config, float delay)
{	
	/* Write the config to ADC first if !NULL */
	if (config) {
		mcp342x_write_config(fd, config);
		if (delay != 0)
			usleep(delay);
	}
	
	struct mcp342x_config data = {};
	mcp342x_read_config(fd, &data);

	int32_t outputcode = data.outputcode;

	/* If MSB is 1, output code is signed. Use 2's complement outputcode */
	if (((data.resolution == CONFIG_RES_18BITS) && (get_msb(data.outputcode, 18 ))) || 
	   ((data.resolution <= CONFIG_RES_16BITS) && (get_msb(data.outputcode, 16)))) {
	   		int nshift = (data.resolution == CONFIG_RES_18BITS) ? 8 : 16;
	   		uint32_t mask = 0xFFFFFFFF >> nshift;
	   		outputcode = -((data.outputcode ^ mask) + 1);
	}
	
	return (float)outputcode * (data.lsb / (float)data.gain);
}

int parse_channels(const char *arg, uint8_t **parsedchannels)
{
	char *s = strdup(arg);
	char *tok = strtok(s, ",");
	uint8_t numchannels = 0;

	while (tok != NULL) {
		int c = atoi(tok);
		if ((c > 0) && (c < 5)) {
			if (*parsedchannels == NULL)
				*parsedchannels = (uint8_t *)malloc(sizeof(uint8_t) * 4);
			*(*parsedchannels + numchannels) = (uint8_t)c;
			numchannels++;
		}
		else {
			numchannels = -1;	
			if (*parsedchannels != NULL)
				free(*parsedchannels);
			break;
		}
		tok = strtok(NULL, ",");
	}

	free(s);
	return numchannels;
}

int main(int argc, char **argv)
{
	mode mode;
	uint8_t configmodeopts = 0, readmodeopts = 0;
	struct mcp342x_config set_config = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	float readinterval = 0;
	int maxreadcount = 0;
	uint8_t *readchannels = NULL;
	uint8_t numreadchannels = 0;
	int ch, bus, addr;
	bool output_csv = false;

	while ((ch = getopt(argc, argv, "b:a:r:c:m:g:i:n:o:")) != -1) {
		switch (ch) {
		case 'b':
			bus = atoi(optarg);
			break;
		case 'a':
			addr = strtol(optarg, NULL, 16);
			break;
		case 'r':
			set_config.resolution = atoi(optarg);
			configmodeopts = 1;
			break;
		case 'c':
			if ((numreadchannels = parse_channels(optarg, 
				&readchannels)) < 0) {
				printf("Invalid '-c' argument: %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'm':
			set_config.mode = atoi(optarg);
			configmodeopts = 1;
			break;
		case 'g':
			set_config.gain = atoi(optarg);
			configmodeopts = 1;
			break;
		case 'i':
			readinterval = atof(optarg);
			readmodeopts = 1;
			break;
		case 'n':
			maxreadcount = atoi(optarg);
			readmodeopts = 1;
			break;
		case 'o': 
			if (strcmp("csv", optarg) == 0) {
				output_csv = true;
			} else {
				printf("Invalid '-o' argument: %s\n", optarg);
				exit(EXIT_FAILURE);
			}	
			break;
		}
	}

	/* The first non-option arg should be the mode */
	char *modearg = argv[optind];
	if (strcmp(modearg, "config") == 0) {
		mode = MODE_CONFIG;
	} else if (strcmp(modearg, "read") == 0) {
		mode = MODE_READ;
	} else if (strcmp(modearg, "reset") == 0) {
		mode = MODE_RESET;
	} else {
		exit(EXIT_FAILURE);
	}

	/* -c is an overloaded parameter, check proper plurality
	 * MODE_CONFIG = channel to set
	 * MODE_READ = channel(s) to read
	 */
	if (mode == MODE_CONFIG) {
		if (numreadchannels > 1) {
			printf("Invalid '-c' argument: You can only specify" 
				" one channel in 'config' mode");
			exit(EXIT_FAILURE);
		} else if (numreadchannels == 1) {
			set_config.channel = readchannels[0];
			configmodeopts = 1;
		}
	}

	/* Check for mode and args mismatches */
	if (((mode == MODE_CONFIG) && readmodeopts) || 
	   ((mode == MODE_READ) && configmodeopts)) 
	   	exit(EXIT_FAILURE);
	   	
	int i2cfd;

	/* Open i2c device */
	char dev[12];
	sprintf(dev, "/dev/i2c-%i", bus);
	if ((i2cfd = open(dev, O_RDWR)) < 0) {
		fprintf(stderr, "Could not open i2c device `%s`: %s\n", dev, 
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Connect to the i2c device */ 
	if (ioctl(i2cfd, I2C_SLAVE, addr) < 0) {
		perror("Could not connect to ADC on i2c bus");
		exit(EXIT_FAILURE);
	}	

	if (mode == MODE_RESET) {
		if (ioctl(i2cfd, I2C_SLAVE, GEN_CALL_ADDR) < 0) {
			perror("Error preparing for general call broadcast");
			exit(EXIT_FAILURE);
		}
		uint8_t cmd = GEN_CALL_CMD_RESET;
		write(i2cfd, &cmd, sizeof(cmd));
		exit(EXIT_SUCCESS);
	}

	/* Read the current config */
	struct mcp342x_config config = {};
	mcp342x_read_config(i2cfd, &config);
	
	if (numreadchannels == 0) {
		numreadchannels = 1;
		readchannels = (uint8_t *)malloc(sizeof(uint8_t));
		readchannels[0] = config.channel;
	}

	if (mode == MODE_CONFIG) {
		if (!configmodeopts) {
			mcp342x_print_config(config);
		} else { 
			mcp342x_apply_config(set_config, &config);
			mcp342x_write_config(i2cfd, &config);
			mcp342x_read_config(i2cfd, &config);
			mcp342x_print_config(config);
		}
	}
	else if (mode == MODE_READ) {
		if (!readmodeopts) {
			if (output_csv) {
				printf("Sample,CH%i\n", config.channel);
				printf("0,%f\n", mcp342x_get_value(i2cfd, 
					NULL, 0));
			} else {
				printf("%f\n", mcp342x_get_value(i2cfd, 
					NULL, 0));
			}
		} else {
			if ((readinterval != 0) && (maxreadcount != 0)) {
				int channelidx = 0;
				if (output_csv) {
					printf("Sample");
					for(int i = 0; i < numreadchannels; i++)
						printf(",CH%i", 
							readchannels[i]);
					printf("\n");
				}

				for (int i = 0; i < maxreadcount; i++) {
					if (numreadchannels > 0) {
						float delay = (numreadchannels > 0) ? 0.004 * 1e6 : 0;
						if (output_csv)
							printf("%i", i);
						for (int c = 0; c < numreadchannels; c++) {
							config.channel = *(readchannels + channelidx);
							float value = mcp342x_get_value(i2cfd, &config, delay);
							if (output_csv)
								printf(",%f", value);
							else
								if (numreadchannels > 1)
									printf("%i: %f\t", config.channel, value);
								else
									printf("%f\t", value);
							channelidx = (channelidx < (numreadchannels - 1)) ? 
									 					 channelidx + 1 : 0;
						}
						printf("\n");
					} else {
						float value = mcp342x_get_value(i2cfd, NULL, 0);
						if (output_csv) 
							printf("%i,%f\n", i, value);
						else
							printf("%f\n", value);
					}

					usleep(readinterval * 1e6);
				}
			}
		}
	}

	if (readchannels != NULL)
		free(readchannels);
}

