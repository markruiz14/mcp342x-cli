#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define ADC_ADDR 0x68

void printbincharpad(char c)
{
	for (int i = 7; i >= 0; --i)
	{
		putchar( (c & (1 << i)) ? '1' : '0' );
	}

	putchar('\n');
}

int main()
{
	int smbus_fp;

	// Open the i2c bus
	if((smbus_fp = open("/dev/i2c-1", O_RDWR)) < 0)
	{
		perror("Could not open i2c bus host controller");
		exit(1);
	}

	// Connect to the adc
	if(ioctl(smbus_fp, I2C_SLAVE, ADC_ADDR) < 0)
	{
		perror("Could not connect to ADC on i2c bus");
		exit(1);
	}	

	// Read 3 bytes for 12bit configuration
	char buf[3];

	if(read(smbus_fp, buf, 3) != 3)
	{
		perror("Could not read bytes from ADC");
	}
	else
	{
		printbincharpad(buf[0]);
		printbincharpad(buf[1]);
		printbincharpad(buf[2]);
	}

	double lsb = 0.001;
	int pga = 1;

	uint16_t outputCode = (buf[0] << 8 ) | buf[1];
	printf("outputCode: %i\n", outputCode);
	
	double value = outputCode * (lsb/pga);
	printf("value: %f\n", value);

	close(smbus_fp);
}
