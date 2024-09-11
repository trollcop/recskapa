# recskapa

Based on recdvb from http://cgi1.plala.or.jp/~sat/

## Description

recskapa - command to record DVB-S/S2 and QAM256 from linux DVB devices, designed
specifically for domestic SkyPerfecTV and SkyPerfecTV Hikari service. Integrates support for
ARIB-STD-B1 decoding via libpcsclite. Only tested on command line and with mirakurun.

## Difference from original?

- Remove http/udp streaming
- Remove IPC to extend recording/retune
- Remove autoconf/update Makefile
- Modify file writing in single blocks (was chunked for http/udp)
- Added configuring tone/voltage to switch JCSAT3A/4B satellites
- Added external channel configuration file instead of hardcoding transponders/services
- Added setting frequency, tone and polarization from command line to avoid duplicating data inside channels.conf
- Added support for SkyPerfecTV Hikari tuning (DVB-C Annex B / J.83B)

- original - [http://cgi1.plala.or.jp/~sat/](http://cgi1.plala.or.jp/~sat/) (link dead as of September 2024)

## How to use (updated)

- See `--help`

- Example with `murakurun config tuners`
```
- name: TBS6903(A)
  types:
    - SKY
  command: recskapa -a 0 -l <satellite> -f <freq> -p <polarity> -b -s - -

- name: TBS6209SE(A)
  types:
    - SKY
  command: recskapa -a 0 -f <freq> -b -s - -
```

 - Note, SkyPerfecTV Hikari places all 20 transponders in VHF/UHF frequency range between 112.8 and 587.3 MHz. Main NIT can be obtained by tuning
   64-QAM DVB-C channel at 375 MHz and doing NIT scanning.
 - Note, it's still possible to use channel configuration file below, but not required for either satellite or hikari service as all tuning details are now provided inside channels.yml.
 - To use channels config, add -c /path/to/channel.conf, omit -l, -f , -p arguments and call the command like so:
 ```
  recskapa -c /path/to/channel.conf -a 0 -b -s <channel> - -
 ```
 - Note, channel.conf is not supported for hikari service.
 
 ## Channel configuration
 
 - See included `skapa.conf`
 - Not required when tuning Skapa Hikari
 
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
