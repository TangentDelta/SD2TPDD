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
Bill Greiman's SdFat library (downloaded from library manager)
```

## Assembly
### Hardware
* Attach the SPI SD card reader to the microcontroller using its SPI bus. Connect the SD card reader's chip select pin to the pin specified by the chipSelect variable (default is pin 4).
* Attach the SPI SD card reader to the microcontroller's power rail.
* Attach the RS232 level shifter to the TX/RX pins of hardware serial port 1 (hardware serial port 0 is the built-in port used for debugging)
* Attach the RS232 level shifter to the microcontroller's power rail.
* Bridge the CTS and RTS pins on the RS232 connector (Required for TS-DOS)
* Bridge the DTR and DSR pins on the RS232 connector (Required for TS-DOS)
### Software
* Load the source file into the Arduino IDE
* Download the SPI and SdFat libraries from the library manager
* Download the SPI and SD libraries from the library manager
* Make any changes if needed
* Compile the code and upload it to the microcontroller

## Notes
If you plan on using TS-DOS, some versions require that you have a DOS100.CO file on the media. This file can be downloaded from here:
http://www.club100.org/nads/dos100.co

If you run into any issues, please let me know!

## To-Do
* (Done!) Move from SD.h to SDfat library for SD card access
* Sub-directory support
* A protocol expansion allowing access to files greater than 64KB in size
* Full NADSBox compatibility
* A command-line that can be accessed from the computer's terminal emulator for quicker file manipulation
* Hayes modem emulation using an ESP8266
* FTP server/client access using an ESP8266
