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
#define CONFIG_MASK_RES		0x0C
#define CONFIG_MASK_GAIN	0x03

#define GEN_CALL_ADDR		0x00
#define GEN_CALL_CMD_RESET	0x06
#define GEN_CALL_CMD_LATCH	0x04
#define GEN_CALL_CMD_CONV	0x08

#define get_msb(n, bits) ((1 << (bits - 1)) & n)

typedef enum {
	MODE_CONFIG,
	MODE_READ,
	MODE_RESET
} MODE;

enum GAIN {
	GAIN_1X = 0,
	GAIN_2X,
	GAIN_4X,
	GAIN_8X
};
typedef uint8_t GAIN;

enum RES {
	RES_12BITS = 0,
	RES_14BITS,
	RES_16BITS,
	RES_18BITS
};
typedef uint8_t RES;

struct mcp342x_config {
	uint8_t ready;
	uint8_t channel;
	uint8_t mode;
	RES resolution;
	GAIN gain;
	uint32_t outputcode;
	float lsb;
};

void print_usage(bool error)
{
	FILE *output = error ? stderr : stdout;
	/*fprintf(output, "mcp342x %s: CLI for configuring and reading data from "
		"the MCP342x family of 18-Bit, multichannel ADC chips with I2C "
		"interface.\n\n"); */
	fprintf(output, "Usage: mcp342x read -b <i2cbus> -a <address> [-c] "
		"[-i] [-n] [-o csv]\n");
	fprintf(output, "       mcp342x config -b <i2cbus> -a <address> [-c] "
		"[-r] [-m] [-g]\n");
	fprintf(output, "       mcp342x reset -b <i2cbus>\n\n");
	
	fprintf(output, "Mode:\n");
	fprintf(output, "    read\tRead the configured channel's value \n"); 
	fprintf(output, "    config\tConfigure the ADC chip. Pass zero parameters to "
		"read and display current configuration\n");
	fprintf(output, "    reset\tBroadcast a RESET general call on the "
		"specified bus\n\n");
	
	fprintf(output, "Read Mode Options:\n");
	fprintf(output, "    -c\t\tSpecified channel(s) to read from "
		"(comma delimited)\n");
	fprintf(output, "    -i\t\tContinuously read the ADC at specified "
		"interval (seconds)\n");
	fprintf(output, "    -n\t\tLimit to specified number of samples\n");
	fprintf(output, "    -o csv\tSet output format to CSV\n\n");

	fprintf(output, "Config Mode Options:\n");
	fprintf(output, "    -c\t\tSet channel to read from\n");
	fprintf(output, "    -r\t\tSet resolution in bits. Valid values are 12, "
		"14, 16, or 18\n");
	fprintf(output, "    -m\t\tSet operation mode. 1 for continuous or 0 for "
		"one-shot conversion mode\n");
	fprintf(output, "    -g\t\tSet channel gain. Valid values are 1, 2, 4, "
		"or 8\n");
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
	case RES_12BITS:
		spsstr = "240 samples/sec (12 bits)";
		break;
	case RES_14BITS:
		spsstr = "60 samples/sec (14 bits)";
		break;
	case RES_16BITS:
		spsstr = "15 samples/sec (16 bits)";
		break;
	case RES_18BITS:
		spsstr = "3.75 samples/sec (18 bits)";
		break;
	}

	/* Gain? */
	char gainstr[3];
	int gain;
	switch (config.gain) {
	case GAIN_1X:
		gain = 1;
		break;
	case GAIN_2X:
		gain = 2;
		break;
	case GAIN_4X:
		gain = 4;
		break;
	case GAIN_8X:
		gain = 8;
		break;
	}
	snprintf(gainstr, sizeof(gainstr), "x%i", gain);
	
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
	config->resolution = (configbits & CONFIG_MASK_RES) >> 2;
	config->gain = (configbits & CONFIG_MASK_GAIN);

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
	if (config->resolution == RES_18BITS) 
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
	byte |= config->gain;

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
	if (((data.resolution == RES_18BITS) && (get_msb(data.outputcode, 18 ))) || 
	   ((data.resolution <= RES_16BITS) && (get_msb(data.outputcode, 16)))) {
	   		int nshift = (data.resolution == RES_18BITS) ? 8 : 16;
	   		uint32_t mask = 0xFFFFFFFF >> nshift;
	   		outputcode = -((data.outputcode ^ mask) + 1);
	}

	return (float)outputcode * (data.lsb / (float)data.gain);
}

int parse_channels(const char *arg, uint8_t **parsedchannels)
{
	char *s = strdup(arg);
	char *tok = strtok(s, ",");
	int numchannels = 0;
	char *end;

	if (*parsedchannels == NULL)
		*parsedchannels = (uint8_t *)malloc(sizeof(uint8_t) * 4);

	while (tok != NULL) {
		int c = strtol(tok, &end, 10);
		if ((c > 0) && (c < 5) && (errno == 0) && (tok != end)) {
			*(*parsedchannels + numchannels) = (uint8_t)c;
			numchannels++;
		} else {
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

void parse_gain_opt(char *optarg, struct mcp342x_config *config) 
{
	char *end;
	int gain = strtol(optarg, &end, 10);

	bool error = false;
	if (((errno != 0) && (gain == 0)) || 
	     (optarg == end)) 
		error = true;
	
	switch (gain) {
	case 1:
		config->gain = GAIN_1X;
		break;
	case 2:
		config->gain = GAIN_2X;
		break;
	case 4:
		config->gain = GAIN_4X;
		break;
	case 8:
		config->gain = GAIN_8X;
		break;
	default:
		error = true;
	}
		
	if (error) {
		printf("Error: Invalid gain setting '%s' for "
			"'-g' argument\n", optarg);
		print_usage(true);
		exit(EXIT_FAILURE);
	}
}

void parse_resolution_opt(char *optarg, struct mcp342x_config *config)
{
	char *end;
	int resolution = strtol(optarg, &end, 10);

	bool error = false;
	if (((errno != 0) && (resolution == 0)) || 
	     (optarg == end)) 
		error = true;
	
	switch (resolution) {
	case 12:
		config->resolution = RES_12BITS;
		break;
	case 14:
		config->resolution = RES_14BITS;
		break;
	case 16:
		config->resolution = RES_16BITS;
		break;
	case 18:
		config->resolution = RES_18BITS;
		break;
	default:
		error = true;
	}
		
	if (error) {
		printf("Error: Invalid resolution setting '%s' for "
			"'-r' argument\n", optarg);
		print_usage(true);
		exit(EXIT_FAILURE);
	}
}

float default_interval(struct mcp342x_config config)
{
	switch (config.resolution) {
	case RES_12BITS:
		return 1.0 / 240.0;
	case RES_14BITS:
		return 1.0 / 60.0;
	case RES_16BITS:
		return 1.0 / 15.0;
	case RES_18BITS:
		return 1.0 / 3.75;
	}
}

int main(int argc, char **argv)
{
	MODE mode;
	uint8_t configmodeopts = 0, readmodeopts = 0;
	struct mcp342x_config set_config = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	float readinterval = 0;
	int maxreadcount = 0;
	uint8_t *readchannels = NULL;
	int numreadchannels = 0;
	int bus = -1;
	int addr = -1;
	bool output_csv = false;
	int ch;
	char *end;

	if (argc  < 2) {
		print_usage(true);
		exit(EXIT_FAILURE);
	}

	while ((ch = getopt(argc, argv, "b:a:r:c:m:g:i:n:o:h")) != -1) {
		switch (ch) {
		case 'b':
			bus = atoi(optarg);
			break;
		case 'a':
			addr = strtol(optarg, &end, 16);
			if (((errno != 0) && (addr == 0)) || 
			     (optarg == end)) {
				printf("Error: Invalid address '%s' for "
					"'-a' argument\n", optarg);
				print_usage(true);
				exit(EXIT_FAILURE);
			}
			break;
		case 'r':
			parse_resolution_opt(optarg, &set_config);
			configmodeopts = 1;
			break;
		case 'c':
			if ((numreadchannels = parse_channels(optarg, 
				&readchannels)) < 0) {
				printf("Error: Invalid channel '%s' for "
					"'-c' argument\n", optarg);
				print_usage(true);
				exit(EXIT_FAILURE);
			}
			break;
		case 'm':
			set_config.mode = strtol(optarg, &end, 10);
			if (((errno != 0) && (set_config.mode == 0)) || 
			     (optarg == end)) {
				printf("Error: Invalid mode '%s' for "
					"'-m' argument\n", optarg);
				print_usage(true);
				exit(EXIT_FAILURE);
			}
			configmodeopts = 1;
			break;
		case 'g':
			parse_gain_opt(optarg, &set_config);	
			configmodeopts = 1;
			break;
		case 'i':
			readinterval = strtof(optarg, &end);
			if (((errno != 0) && (readinterval == 0)) || 
			     (optarg == end)) {
				printf("Error: Invalid interval '%s' for "
					"'-i' argument\n", optarg);
				print_usage(true);
				exit(EXIT_FAILURE);
			}
			readmodeopts = 1;
			break;
		case 'n':
			maxreadcount = strtol(optarg, &end, 10);
			if (((errno != 0) && (maxreadcount == 0)) || 
			     (optarg == end)) {
				printf("Error: Invalid max samples value '%s' "
					"for '-n' argument\n", optarg);
				print_usage(true);
				exit(EXIT_FAILURE);
			}
			readmodeopts = 1;
			break;
		case 'o': 
			if (strcmp("csv", optarg) == 0) {
				output_csv = true;
			} else {
				printf("Error: Invalid '-o' argument '%s'\n", 
					optarg);
				exit(EXIT_FAILURE);
			}	
			break;
		case 'h':
			print_usage(false);
			exit(EXIT_SUCCESS);
		default:
			print_usage(true);
			exit(EXIT_FAILURE);
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
		printf("Error: Unrecognized mode '%s'\n", modearg);
		print_usage(true);
		exit(EXIT_FAILURE);
	}

	/* -b and -a are required parameters*/
	if ((mode == MODE_CONFIG) || (mode == MODE_READ)) {
		bool error = false;
		if (bus == -1) {
			printf("Error: Missing required parameter "
				"'-b <i2cbus>'\n");
			error = true;
		}

		if (addr == -1) {
			printf("Error: Missing required parameter "
				"'-a <address>'\n");
			error = true;
		}

		if(error) {
			print_usage(true);
			exit(EXIT_FAILURE);
		}
	}

	/* -c is an overloaded parameter, check proper plurality
	 * MODE_CONFIG = channel to set
	 * MODE_READ = channel(s) to read
	 */
	if (mode == MODE_CONFIG) {
		if (numreadchannels > 1) {
			printf("Error: Invalid '-c' argument: You can only " 
				"specify one channel\n");
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
	if (mcp342x_read_config(i2cfd, &config) < 0) {
		perror("Error reading from ADC");
		exit(EXIT_FAILURE);
	}
	
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
	} else if (mode == MODE_READ) {
		/* CSV header */
		if (output_csv) {
			printf("Sample");
			for (int i = 0; i < numreadchannels; i++)
				printf(",CH%i", readchannels[i]);
			printf("\n");
		}
		
		if (readinterval == 0)
			readinterval = default_interval(config);
		else if (maxreadcount == 0)
			maxreadcount = INT_MAX;

		int sample = 0;
		do {
			/* Sequential channel reads requires a delay 
			   since we're doing a config write to switch
			   the channel before doing a read operation */
			float delay = (numreadchannels > 0) ? 0.004 * 1e6 : 0;

			if (output_csv)
				printf("%i", sample);

			for (int i = 0; i < numreadchannels; i++) {
				config.channel = *(readchannels + i);
				float value = mcp342x_get_value(i2cfd, &config, 
						delay);

				if (output_csv)
					printf(",%f", value);
				else
					if (numreadchannels > 1)
						printf("CH%i: %f\t", 
							config.channel, 
							value);
					else
						printf("%f\t", value);
			}

			printf("\n");
			usleep(readinterval * 1e6);
			sample++;
		} while ((sample < maxreadcount) || (maxreadcount == INT_MAX));
	}

	if (readchannels != NULL)
		free(readchannels);
}

