## SidBerry ##
### USBSID-Pico Music player for MOS SID chip (6581/8580), [SIDKick-pico](https://github.com/frntc/SIDKick-pico) or SwinSID like replacements ###
[USBSID-Pico](https://github.com/LouDnl/USBSID-Pico)
#### Original Author and repo of SidBerry ####
[@gianlucag](https://github.com/gianlucag) ~ [SidBerry](https://github.com/gianlucag/SidBerry)

## USBSID-Pico ##
This repo contains an adaptation of SidBerry that includes playing SID files over USB on a Linux PC (Windows if you compile on Windows - no support).

### Dependencies for building
**USBSID-Pico** requires libusb

## Building SidBerry ##
Examples:
```shell
git clone --recurse-submodules git@github.com:LouDnl/SidBerry.git
# Compile with
cmake -S . -B build && cmake --build build --parallel $(nproc)
# Install with
cp build/usbsidberry ~/.local/bin/
```

# The original [README](README-original.md) by [@gianlucag](https://github.com/gianlucag/SidBerry)
