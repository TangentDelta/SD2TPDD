/*
 * SD2TPDD
 * A TPDD emulator for the Arduino Mega that uses an SD card for mass storage.
 * Written by Jimmy Pettit
 * 07/21/2018
 */

#include <SPI.h>
#include <SdFat.h>

SdFat SD;

File root;  //Root file for filesystem reference
File entry; //Moving file entry for the emulator
File tempEntry; //Temporary entry for moving files

const byte chipSelect = 4; //SD Card chip select pin

byte head = 0x00;  //Head index
byte tail = 0x00;  //Tail index

byte checksum = 0;  //Global variable for checksum calculation

byte state = 0; //Emulator command reading state

byte dataBuffer[256]; //Data buffer for commands
byte fileBuffer[0x80]; //Data buffer for file reading

char refFileName[25] = "";  //Reference file name for emulator
char tempRefFileName[25] = ""; //Second reference file name for renaming
char entryName[24] = "";  //Entry name for emulator
int directoryBlock = 0; //Current directory block for directory listing
bool append = false;  //SD library lacks an append mode, keep track of it with a flag

void setup() {
  Serial.begin(19200);  //Start the debug serial port
  Serial1.begin(19200);  //Start the main serial port

  clearBuffer(dataBuffer, 256); //Clear the data buffer

  Serial.print("Initializing SD card...");

  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  root = SD.open("/");  //Create the root filesystem entry

  printDirectory(root,0); //Print directory for debug purposes

  root.close();

}

/*
 * 
 * General misc. routines
 * 
 */

void printDirectory(File dir, int numTabs) { //Copied code from the file list example for debug purposes
  char fileName[24] = "";
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    entry.getName(fileName,24);
    Serial.print(fileName);
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.fileSize(), DEC);
    }
    entry.close();
  }
}

void clearBuffer(byte* buffer, int bufferSize){ //Fills the buffer with 0x00
  for(int i=0; i<bufferSize; i++){
    buffer[i] = 0x00;
  }
}

/*
 * 
 * TPDD Port misc. routines
 * 
 */

void tpddWrite(char c){  //Outputs char c to TPDD port and adds to the checksum
  checksum += c;
  Serial1.write(c);
}

void tpddSendChecksum(){  //Outputs the checksum to the TPDD port and clears the checksum
  Serial1.write(checksum^0xFF);
  checksum = 0;
}


/*
 * 
 * TPDD Port return routines
 * 
 */
 
void return_normal(byte errorCode){ //Sends a normal return to the TPDD port with error code errorCode
  Serial.print("R:Norm ");
  Serial.println(errorCode, HEX);

  tpddWrite(0x12);  //Return type (normal)
  tpddWrite(0x01);  //Data size (1)
  tpddWrite(errorCode); //Error code
  tpddSendChecksum(); //Checksum
}

void return_reference(){  //Sends a reference return to the TPDD port
  tpddWrite(0x11);  //Return type (reference)
  tpddWrite(0x1C);  //Data size (1C)

  clearBuffer(tempRefFileName,24);  //Clear the reference file name buffer

  entry.getName(tempRefFileName,24);  //Save the current file entry's name to the reference file name buffer

  for(int i=0; i<24; i++){
    tpddWrite(tempRefFileName[i]);  //Write the reference file name to the TPDD port
  }

  tpddWrite(0x00);  //Attribute, unused
  tpddWrite((byte)((entry.fileSize()&0xFF00)>>8));  //File size most significant byte
  tpddWrite((byte)(entry.fileSize()&0xFF)); //File size least significant byte
  tpddWrite(0xFF);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum

  Serial.println("R:Ref");
}

void return_blank_reference(){  //Sends a blank reference return to the TPDD port
  tpddWrite(0x11);  //Return type (reference)
  tpddWrite(0x1C);  //Data size (1C)

  for(int i=0; i<24; i++){
    tpddWrite(0x00);  //Write the reference file name to the TPDD port
  }

  tpddWrite(0x00);  //Attribute, unused
  tpddWrite(0x00);  //File size most significant byte
  tpddWrite(0x00); //File size least significant byte
  tpddWrite(0xFF);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum

  Serial.println("R:BRef");
}

/*
 * 
 * TPDD Port command handler routines
 * 
 */

void command_reference(){ //Reference command handler
  byte searchForm = dataBuffer[(byte)(tail+29)];  //The search form byte exists 29 bytes into the command
  byte refIndex = 0;  //Reference file name index

  root = SD.open("/");
  
  Serial.print("SF:");
  Serial.println(searchForm,HEX);
  
  if(searchForm == 0x00){ //Request entry by name
    for(int i=4; i<28; i++){  //Put the reference file name into a buffer
        if(dataBuffer[(byte)(tail+i)]!=0x20){ //If the char pulled from the command is not a space character (0x20)...
          refFileName[refIndex++]=dataBuffer[(byte)(tail+i)]; //write it into the buffer and increment the index. 
        }
    }
    refFileName[refIndex]=0x00; //Terminate the file name buffer with a null character

    Serial.print("Ref: ");
    Serial.println(refFileName);

    if(SD.exists(refFileName)){ //If the file exists on the SD card...
      entry=SD.open(refFileName); //...open it...
      return_reference(); //send a refernce return to the TPDD port with its info...
      entry.close();  //...close the entry.
    }else{  //If the file does not exist...
      return_blank_reference();
    }
    
  }else if(searchForm == 0x01){ //Request first directory block
    directoryBlock = 0; //Set the current directory entry index to 0

    root.rewindDirectory(); //Pull back to the begining of the directory
    entry = root.openNextFile();  //Open the first entry
    return_reference(); //send a reference return to the TPDD port with it info
    entry.close();  //Close the open entry
    
  }else if(searchForm == 0x02){ //Request next directory block
    directoryBlock++; //Increment the directory entry index
    
    root.rewindDirectory(); //Pull back to the begining of the directory
    for(int i=0; i<directoryBlock; i++){  //skip to the current entry offset by the index
      root.openNextFile();
    }
    entry = root.openNextFile();  //Open the entry
    return_reference(); //Send the reference info to the TPDD port
    entry.close();  //Close the entry
    
  }else{  //Parameter is invalid
    return_normal(0x36);  //Send a normal return to the TPDD port with a parameter error
  }

  root.close();
}

void command_open(){  //Opens an entry for reading, writing, or appending
  byte rMode = dataBuffer[(byte)(tail+4)];  //The access mode is stored in the 5th byte of the command
  entry.close();
  
  switch(rMode){
    case 0x01: entry = SD.open(refFileName, FILE_WRITE); append=false; break; //Write
    case 0x02: entry = SD.open(refFileName, FILE_WRITE); append=true; break;  //Append, set the append flag
    case 0x03: entry = SD.open(refFileName, FILE_READ); append=false; break;  //Read
  }

  if(!entry){
    Serial.println("Error: File could not be opened!");
  }
  
  if(SD.exists(refFileName)){ //If the file actually exists...
    return_normal(0x00);  //...send a normal return with no error.
  }else{  //If the file doesn't exist...
    return_normal(0x10);  //...send a normal return with a "file does not exist" error.
  }
}

void command_close(){ //Closes the currently open entry
  entry.close();  //Close the entry
  return_normal(0x00);  //Normal return with no error
}

void command_read(){  //Read a block of data from the currently open entry
  int bytesRead = entry.read(fileBuffer, 0x80); //Try to pull 128 bytes from the 
  //Serial.print("A: ");
  //Serial.println(entry.available(),HEX);

  if(bytesRead > 0){
    tpddWrite(0x10);
    tpddWrite(bytesRead);
    for(int i=0; i<bytesRead; i++){
      tpddWrite(fileBuffer[i]);
    }
    tpddSendChecksum();
  }else{
    return_normal(0x3F);  //...send a normal return with an end-of-file error.
  }
}

void command_write(){ //Write a block of data from the command to the currently open entry
  byte commandDataLength = dataBuffer[(byte)(tail+3)];
  Serial.print("L: ");
  Serial.println(commandDataLength);

  for(int i=0; i<commandDataLength; i++){
    if(append){
      entry.print(dataBuffer[(byte)(tail+4+i)]);
    }else{
      entry.write(dataBuffer[(byte)(tail+4+i)]);
    }
  }

  //Serial.println(entry.getWriteError());
  
  //entry.flush();  //Flush the data to be written to the SD card to prevent corruption
  return_normal(0x00);  //Send a normal return to the TPDD port with no error
}

void command_delete(){  //Delete the currently open entry
  entry.close();  //Close the entry
  SD.remove(refFileName); //Delete the entry based on the name
  return_normal(0x00);  //Send a normal return with no error
}

void command_format(){  //Not implemented
  return_normal(0x00);
}

void command_status(){  //Drive status
  return_normal(0x00);
}

void command_condition(){ //Not implemented
  return_normal(0x00);
}

void command_rename(){  //Renames the currently open entry
  byte refIndex = 0;  //Temporary index for the reference name
  
  for(int i=4; i<28; i++){  //Loop through the command's data block, which contains the new entry name
      if(dataBuffer[(byte)(tail+i)]!=0x20){ //If the current character is not a space (0x20)...
        tempRefFileName[refIndex++]=dataBuffer[(byte)(tail+i)]; //...copy the character to the temporary reference name and increment the pointer.
      }
  }
  
  tempRefFileName[refIndex]=0x00; //Terminate the temporary reference name with a null character

  SD.rename(refFileName, tempRefFileName);  //Rename the file
  
  return_normal(0x00);  //Send a normal return to the TPDD port with no error
}

/*
 * 
 * Main code loop
 * 
 */

void loop() {
  byte rType = 0; //Current request type (command type)
  byte rLength = 0; //Current request length (command length)
  byte diff = 0;  //Difference between the head and tail buffer indexes

  state = 0; //0 = waiting for command 1 = waiting for full command 2 = have full command
  
  while(state<2){ //While waiting for a command...
    while (Serial1.available() > 0){  //While there's data to read from the TPDD port...
      dataBuffer[head++]=(byte)Serial1.read();  //...pull the character from the TPDD port and put it into the command buffer, increment the head index...
      if(tail==head){ //...if the tail index equals the head index (a wrap-around has occoured! data will be lost!)
        tail++; //...increment the tail index to prevent the command size from overflowing.
      }
      //Serial.print((byte)(head-1),HEX); //Debug code
      //Serial.print("-");
      //Serial.print(tail,HEX);
      //Serial.print((byte)(head-tail),HEX);
      //Serial.print(":");
      //Serial.print(dataBuffer[head-1],HEX);
      //Serial.print(";");
      //Serial.println((dataBuffer[head-1]>=0x20)&&(dataBuffer[head-1]<=0x7E)?(char)dataBuffer[head-1]:' ');
    }

    diff=(byte)(head-tail); //...set the difference between the head and tail index (number of bytes in the buffer)

    if(state == 0){ //...if we're waiting for a command...
      if(diff >= 4){  //...if there are 4 or more characters in the buffer...
        if(dataBuffer[tail]=='Z' && dataBuffer[(byte)(tail+1)]=='Z'){ //...if the buffer's first two characters are 'Z' (a TPDD command)
          rLength = dataBuffer[(byte)(tail+3)]; //...get the command length...
          rType = dataBuffer[(byte)(tail+2)]; //...get the command type...
          state = 1;  //...set the state to "waiting for full command".
        }else{  //...if the first two characters are not 'Z'...
          tail=tail+(tail==head?0:1); //...move the tail index forward to the next character, stop if we reach the head index to prevent an overflow.
        }
      }
    }

    if(state == 1){ //...if we're waiting for the full command to come in...
      if(diff>rLength+4){ //...if the amount of data in the buffer satisfies the command length...
          state = 2;  //..set the state to "have full command".
        }
    }
  } 

  Serial.print(tail,HEX); //Debug code that displays the tail index in the buffer where the command was found...
  Serial.print("=");
  Serial.print("T:"); //...the command type...
  Serial.print(rType, HEX);
  Serial.print("|L:");  //...and the command length.
  Serial.println(rLength, HEX);



  switch(rType){  //Select the command handler routine to jump to based on the command type
    case 0x00: command_reference(); break;
    case 0x01: command_open(); break;
    case 0x02: command_close(); break;
    case 0x03: command_read(); break;
    case 0x04: command_write(); break;
    case 0x05: command_delete(); break;
    case 0x06: command_format(); break;
    case 0x07: command_status(); break;
    case 0x0C: command_condition(); break;
    case 0x0D: command_rename(); break;
    default: return_normal(0x36); break;  //Send a normal return with a parameter error if the command is not implemented
  }
  //Serial.print(head,HEX);
  //Serial.print(":");
  //Serial.print(tail,HEX);
  //Serial.print("->");
  tail = tail+rLength+5;  //Increment the tail index past the previous command
  //Serial.println(tail,HEX);
  
}
