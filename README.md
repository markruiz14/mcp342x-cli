# mcp342x-cli

A command line tool for configuring and reading data from the MCP342x family of 2-channel and 4-channel i2c ADC chips.

**Features**    

* Autodetect i2c bus and chip address
* Fast (sequential) multi-channel sampling
* CSV output option
* Option to read `n` samples at interval `i`

## Usage Examples
##### Read current ADC configuration
 `$ ./mcp342x config`

##### Set channel, sampling bit rate, and gain
 `$ ./mcp342x config -c 1 -r 18 -g 2`

##### Sample configured channel 100 times every tenth of a second
 `$ ./mcp342x read -n 100 -i 0.1`

##### Sample configured channel 100 times every tenth of a second
 `$ ./mcp342x read -n 100 -i 0.1`
 
##### Sample multiple channels 500 times every hundredth of a second
 `$ ./mcp342x read --c 1,2 -n 500 -i 0.01`

##### Sample multiple channels and output to CSV
 `$ ./mcp342x read --c 1,2 -n 500 -i 0.01 -o csv > samples.csv`
  
## Multi-Channel Examples
The following samples were taken using a MCP3424. Output of waveform generator was simultaneously connected to all 4 input channels.

---
#### Sine
![Sinc function](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/sine-all-channels.svg)
`$ ./mcp342x -c 1,2,3,4 -n 100 -i 0.004`

---
#### Sinc
![Sinc function](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/sinc.svg)
`$ ./mcp342x -c 1,2,3,4 -n 500 -i 0.004`

---
#### Radar
![Sinc function](http://s3.amazonaws.com/static.markruiz.com/mcp342x-cli/radar.svg)
`$ ./mcp342x -c 1,2,3,4 -n 500 -i 0.004`

## Prerequisites
* Linux w/ installed packages:
  * i2c-tools

`apt-get install i2c-tools` or `yum install i2c-tools`

## Compiling
`$ make`




