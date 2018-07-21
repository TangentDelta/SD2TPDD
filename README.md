# SD2TPDD
A hardware emulator of the Tandy Portable Disk Drive using an SD card for mass storage
## Verbose Description
The SD2TPDD is a project that aims to provide an easy-to-use, cheap, and reliable mass storage solution for the TRS-80 Model 100 series of computers. 

## Requirements
### Hardware
```
Arduino Mega or compatible with two hardware serial ports
SPI SD card reader with logic level shifting
RS232 level shifter for the TPDD port going to the computer (MAX232 or MAX3232 prefered!)
```

### Software
```
Arduino IDE
Arduino SPI library (downloaded from library manager)
Arduino SD library (downloaded from library manager)
```

## Assembly
### Hardware
*Attach the SPI SD card reader to the microcontroller using its SPI bus. Connect the SD card reader's chip select pin to the pin specified by the chipSelect variable (default is pin 4).
*Attach the SPI SD card reader to the microcontroller's power rail.
*Attach the RS232 level shifter to the TX/RX pins of hardware serial port 1 (hardware serial port 0 is the built-in port used for debugging)
*Attach the RS232 level shifter to the microcontroller's power rail.
### Software
*Load the source file into the Arduino IDE
*Download the SPI and SD libraries from the library manager
*Make any changes if needed
*Compile the code and upload it to the microcontroller

## Notes
If you plan on using TS-DOS, some versions require that you have a DOS100.CO file on the media. This file can be downloaded from here:
http://www.club100.org/nads/dos100.co

If you run into any issues, please let me know!
