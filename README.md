# mcp342x-cli

A command line tool for configuring and reading data from the MCP342x family of 2-channel and 4-channel i2c ADC chips.

**Features**    
* Fast (sequential) multi-channel sampling
* CSV output option
* Option to read `n` samples at interval `i`

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




