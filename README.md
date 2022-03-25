# Raspberry Pi Virtual Breadboard EDGEY Examples

## Getting started

### Setup Raspi Pico Build system 

[pico-sdk](https://github.com/raspberrypi/pico-sdk) for information on getting up and running.

[How to setup Raspberry Pi Pico C/C++ SDK in Window10](https://www.hackster.io/lawrence-wiznet-io/how-to-setup-raspberry-pi-pico-c-c-sdk-in-window10-f2b816) 

### Clone this repo into the SDK directory

The following **Assumes** the SDKC is installed in C:\RP2040

<code>
cd C:\RP2040
git clone https://github.com/virtualbreadboard/vbb-raspi-pico.git
</code>

### Build 

<code>
cd C:\RP2020\vbb-raspi-pico
mkdir build
cd build
cmake -G "NMake Makefiles" ..
nmake
</code>

### Firmware files are located in the build directory

C:\RP2020\vbb-raspi-pico\build\tinyML\edge-rps\ei_rp2040_firmware.uf2

### To Install a firmware file

1. Remove from EDGEY socket
2. Disconnect Raspi USB
3. Hold Down Reset Button
4. Reconnect Raspi to show boot file system
5. Drag and drop uf2 file into boot file system
