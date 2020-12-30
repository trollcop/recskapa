# recskapa

Based on recdvb from http://cgi1.plala.or.jp/~sat/

## Description

recskapa - command to record DVB-S/S2 from linux DVB devices, designed
specifically for domestic SkyPerfecTV service. Integrates support for
ARIB-STD-B1 decoding via libpcsclite. Only tested on command line and 
with mirakurun.

## Difference from original?

- Remove http/udp streaming
- Remove IPC to extend recording/retune
- Remove autoconf/update Makefile
- Modify file writing in single blocks (was chunked for http/udp)
- Added configuring tone/voltage to switch JCSAT3A/4B satellites
- Added external channel configuration file instead of hardcoding transponders/services
- Added setting frequency, tone and polarization from command line to avoid duplicating data inside channels.conf

- original - [http://cgi1.plala.or.jp/~sat/](http://cgi1.plala.or.jp/~sat/)

## How to use (updated)

- See `--help`

- Example with `murakurun config tuners`
```
- name: TBS6903(A)
  types:
    - SKY
  command: recskapa -a 0 -l <satellite> -f <freq> -p <polarity> -b -s - -
```

 - Note, it's still possible to use channel configuration file below, but not required as all tuning details are now provided inside channels.yml.
 - To use channels config, add -c /path/to/channel.conf, omit -l, -f , -p arguments and call the command like so:
 ```
  recskapa -c /path/to/channel.conf -a 0 -b -s <channel> - -
 ```
 
 ## Channel configuration
 
 - See included `skapa.conf`
 
 ```
# JCSAT 4B    (124) Tone
# JCSAT 3A    (128) No tone
#
# Current as of 2020/07
#
# File format:
#
# Name:Frequency:Options:Symbol Rate
# Options:
# H   Horizontal
# V   Vertical
# T   Tone (tune to 124, default tune to 128)
# 1   Set DELSYS to DVB-S (default DVB-S2)
#
CH608:12523:H:23303
CH592:12568:VT:23303
```
