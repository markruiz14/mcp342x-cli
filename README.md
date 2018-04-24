# mcp342x-cli

A command line tool for configuring and reading data from the MCP342x family of 18-Bit, multichannel ADC chips with I2C interface.

**:sparkles:Features:sparkles:**    

* Autodetect I2C bus and chip address
* Fast (sequential) multi-channel sampling
* CSV output option
* Option to read at interval `i` and limit to `n` samples

## Usage Examples

```bash
# Read current ADC configuration
$ ./mcp342x config
```

```bash
# Set to channel 1, sampling rate to 18bits, and gain to 2x
$ ./mcp342x config -c 1 -r 18 -g 2
```

```bash
# Sample configured channel every 0.1 seconds, limit to 100 samples
$ ./mcp342x read -n 100 -i 0.1
```

```bash 
# Sample channels 1 and 2 every 0.01 seconds, limit to 500 samples
$ ./mcp342x read --c 1,2 -n 500 -i 0.01
```

```bash
# Same as above, but output to CSV format. Redirect to samples.csv
$ ./mcp342x read --c 1,2 -n 500 -i 0.01 -o csv > samples.csv
```
  
## Multi-Channel Examples
The following samples were taken using a MCP3424. Output of waveform generator was simultaneously connected to all 4 input channels.

---
![Sinc function](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/sine-all-channels.svg)

```bash
# Sampled every 0.004 seconds, 100 samples
$ ./mcp342x read -c 1,2,3,4 -n 100 -i 0.004
```

---
![Sinc function](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/sinc.svg)

```bash
# Sampled every 0.004 seconds, 500 samples
$ ./mcp342x read -c 1,2,3,4 -n 500 -i 0.004
```

---
![Sinc function](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/radar.svg)

```bash
# Sampled every 0.004 seconds, 500 samples
$ ./mcp342x read -c 1,2,3,4 -n 500 -i 0.004
```

## Prerequisites
* Linux w/ installed packages:
  * i2c-tools

`apt-get install i2c-tools` or `yum install i2c-tools`

## Compiling
```bash
$ make
```
```bash
$ make install
```
## Resources
* [MCP342x Datasheet](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/radar.svg)
* [CHIP Microhat](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/radar.svg)


