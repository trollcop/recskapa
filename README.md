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

- original - [http://cgi1.plala.or.jp/~sat/](http://cgi1.plala.or.jp/~sat/)

## How to use

- See `--help`

- Example with `murakurun config tuners`
```
- name: TBS6903(A)
  types:
    - SKY
  command: recskapa -c /usr/local/etc/skapa.conf -a 0 -l <satelite> -b -s <channel> - -
```

 - Note, -l <satelite> parameter is currently ignored (satellite selection is done by channel config file, below
 
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
