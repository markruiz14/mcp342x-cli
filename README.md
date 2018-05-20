# mcp342x-cli

A command line tool for configuring and reading data from the MCP342x family of 18-Bit, multichannel ADC chips with I2C interface.

**:sparkles:Features:sparkles:**    

* Fast (sequential) multi-channel sampling
* CSV output option
* Option to sample at specific interval and limit to a number of samples
* Broadcast general call commands to bus
  * RESET
  * POR (Power On Reset)
  * LATCH 

## Usage Examples

```bash
# Read current ADC config on I2C bus 1, address 0x68
$ ./mcp342x config -b 1 -a 0x68
Ready: No
Channel: 1
Conversion mode: Continuous
Sample rate: 240 samples/sec (12 bits)
Gain: x1
```

```bash
# Configure ADC to use 18 bit resolution
$ ./mcp342x config -b 1 -a 0x68 -r 18
Ready: No
Channel: 1
Conversion mode: Continuous
Sample rate: 3.75 samples/sec (18 bits)
Gain: x1
```

```bash
# Read currently configured ADC channel
$ ./mcp342x read -d 1 -a 0x68
CH1: 1.487156
```

```bash 
 # Read ADC channels 1,2,3,4
$ ./mcp342x read -d 1 -a 0x68 -c 1,2,3,4
CH1: 0.674594	CH2: 0.674594	CH3: 0.674594	CH4: 0.674594
```

```bash
# Sample ADC channel 1,2,3,4 every 0.004 seconds, up to 10 samples. Output CSV.
$ ./mcp342x read -b 1 -a 0x68 read -i 0.004 -n 10 -o csv
Sample,CH1,CH2,CH3,CH4
0,1.864000,1.857000,1.851000,1.846000
1,1.838000,1.831000,1.826000,1.820000
2,1.810000,1.805000,1.799000,1.792000
3,1.784000,1.777000,1.770000,1.762000
4,1.755000,1.747000,1.739000,1.733000
5,1.724000,1.715000,1.709000,1.702000
6,1.691000,1.684000,1.677000,1.669000
7,1.658000,1.651000,1.643000,1.634000
8,1.625000,1.617000,1.607000,1.600000
9,1.590000,1.580000,1.572000,1.564000
```
  
## Multi-Channel Examples
The following samples were taken using a MCP3424. Output of waveform generator was simultaneously connected to all 4 input channels.

---
![Sinc function](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/sine-all-channels2.svg)

```bash
# Sampled every 0.004 seconds, 100 samples
$ ./mcp342x read -b 1 -a 0x68 -c 1,2,3,4 -n 100 -i 0.004
```

---
![Sinc function](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/sinc2.svg)

```bash
# Sampled every 0.004 seconds, 500 samples
$ ./mcp342x read -b 1 -a 0x68 -c 1,2,3,4 -n 500 -i 0.004
```

---
![Sinc function](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/radar2.svg)

```bash
# Sampled every 0.004 seconds, 500 samples
$ ./mcp342x read -b 1 -a 0x68  -c 1,2,3,4 -n 500 -i 0.004
```

## Prerequisites
* Linux w/ installed packages:
  * build-essential	
  * i2c-tools

## Compiling
```bash
$ make
```
## Resources
* [MCP342x Datasheet](http://ww1.microchip.com/downloads/en/DeviceDoc/22088c.pdf)


