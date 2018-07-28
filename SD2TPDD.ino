/*
 * SD2TPDD  V0.2
 * A TPDD emulator for the Arduino Mega that uses an SD card for mass storage.
 * Written by Jimmy Pettit
 * 07/27/2018
 */

#include <SPI.h>
#include <SdFat.h>

SdFat SD; //SD card object

File root;  //Root file for filesystem reference
File entry; //Moving file entry for the emulator
File tempEntry; //Temporary entry for moving files

const byte chipSelect = 4; //SD Card chip select pin

byte head = 0x00;  //Head index
byte tail = 0x00;  //Tail index

byte checksum = 0;  //Global variable for checksum calculation

byte state = 0; //Emulator command reading state
bool DME = false; //TS-DOS DME mode flag

byte dataBuffer[256]; //Data buffer for commands
byte fileBuffer[0x80]; //Data buffer for file reading

char refFileName[25] = "";  //Reference file name for emulator
char refFileNameNoDir[25] = ""; //Reference file name for emulator with no ".<>" if directory
char tempRefFileName[25] = ""; //Second reference file name for renaming
char entryName[24] = "";  //Entry name for emulator
int directoryBlock = 0; //Current directory block for directory listing
bool append = false;  //SD library lacks an append mode, keep track of it with a flag
char directory[60] = "/";
byte directoryDepth = 0;
char tempDirectory[60] = "/";

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

  root = SD.open(directory);  //Create the root filesystem entry

  printDirectory(root,0); //Print directory for debug purposes

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

void directoryAppend(char* c){  //Copy a null-terminated char array to the directory array
  bool terminated = false;
  int i = 0;
  int j = 0;
  
  while(directory[i] != 0x00){  //Jump i to first null character
    i++;
  }

  while(!terminated){
    directory[i++] = c[j++];
    terminated = c[j] == 0x00;
  }
  //Serial.println(directory);
}

void upDirectory(){ //Removes the top-most entry in the directoy path
  int j = sizeof(directory);

  while(directory[j] == 0x00){ //Jump to first non-null character
    j--;
  }

  if(directory[j] == '/' && j!= 0x00){  //Strip away the slash character
    directory[j] = 0x00;
  }

  while(directory[j] != '/'){ //Move towards the front of the array until a slash character is encountered...
    directory[j--] = 0x00;  //...set everything along the way to null characters
  }
}

void copyDirectory(){ //Makes a copy of the working directory to a scratchpad
  for(int i=0; i<sizeof(directory); i++){
    tempDirectory[i] = directory[i];
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

void tpddWriteString(char* c){  //Outputs a null-terminated char array c to the TPDD port
  int i = 0;
  while(c[i]!=0){
    checksum += c[i];
    Serial1.write(c[i]);
    i++;
  }
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
  //Serial.print("R:Norm ");
  //Serial.println(errorCode, HEX);

  tpddWrite(0x12);  //Return type (normal)
  tpddWrite(0x01);  //Data size (1)
  tpddWrite(errorCode); //Error code
  tpddSendChecksum(); //Checksum
}

void return_reference(){  //Sends a reference return to the TPDD port
  byte term = 6;
  bool terminated = false;
  tpddWrite(0x11);  //Return type (reference)
  tpddWrite(0x1C);  //Data size (1C)

  clearBuffer(tempRefFileName,24);  //Clear the reference file name buffer

  entry.getName(tempRefFileName,24);  //Save the current file entry's name to the reference file name buffer
  
  if(DME && entry.isDirectory()){ //      !!!Tacks ".<>" on the end of the return reference if we're in DME mode and the reference points to a directory
    for(int i=0; i < 7; i++){ //Find the end of the directory's name by looping through the name buffer
      if(tempRefFileName[i] == 0x00){
        term = i; //and setting a termination index to the offset where the termination is encountered
      }
    }
    tempRefFileName[term++] = '.';  //Tack the expected ".<>" to the end of the name
    tempRefFileName[term++] = '<';
    tempRefFileName[term++] = '>';

    for(int i=term; i<24; i++){ //Fill the rest of the reference name with null characters
      tempRefFileName[i] = 0x00;
    }
    term = 6; //Reset the termination index to prepare for the next check
  }

  

  for(int i=0; i<6; i++){ //      !!!Pads the name of the file out to 6 characters using space characters
    if(term == 6){  //Perform these checks if term hasn't changed
      if(tempRefFileName[i]=='.'){
        term = i;   //If we encounter a '.' character, set the temrination pointer to the current offset and output a space character instead
        tpddWrite(' ');
      }else{
        tpddWrite(tempRefFileName[i]);  //If we haven't encountered a period character, output the next character
      }
    }else{
      tpddWrite(' '); //If we did find a period character, write a space character to pad the reference name
    }
  }

  for(int i=0; i<18; i++){  //      !!!Outputs the file extension part of the reference name starting at the offset found above
    tpddWrite(tempRefFileName[i+term]);
  }

  tpddWrite(0x00);  //Attribute, unused
  tpddWrite((byte)((entry.fileSize()&0xFF00)>>8));  //File size most significant byte
  tpddWrite((byte)(entry.fileSize()&0xFF)); //File size least significant byte
  tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum

  //Serial.println("R:Ref");
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
  tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum

  //Serial.println("R:BRef");
}

void return_parent_reference(){
  tpddWrite(0x11);
  tpddWrite(0x1C);

  tpddWriteString("PARENT.<>");
  for(int i=9; i<24; i++){  //Pad the rest of the data field with null characters
    tpddWrite(0x00);
  }

  tpddWrite(0x00);  //Attribute, unused
  tpddWrite(0x00);  //File size most significant byte
  tpddWrite(0x00); //File size least significant byte
  tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum
}

/*
 * 
 * TPDD Port command handler routines
 * 
 */

void command_reference(){ //Reference command handler
  byte searchForm = dataBuffer[(byte)(tail+29)];  //The search form byte exists 29 bytes into the command
  byte refIndex = 0;  //Reference file name index
  
  //Serial.print("SF:");
  //Serial.println(searchForm,HEX);
  
  if(searchForm == 0x00){ //Request entry by name
    for(int i=4; i<28; i++){  //Put the reference file name into a buffer
        if(dataBuffer[(byte)(tail+i)]!=0x20){ //If the char pulled from the command is not a space character (0x20)...
          refFileName[refIndex++]=dataBuffer[(byte)(tail+i)]; //write it into the buffer and increment the index. 
        }
    }
    refFileName[refIndex]=0x00; //Terminate the file name buffer with a null character

    //Serial.print("Ref: ");
    //Serial.println(refFileName);

    if(DME){  //        !!!Strips the ".<>" off of the reference name if we're in DME mode
      if(strstr(refFileName, ".<>") != 0){
        for(int i=0; i<24; i++){  //Copies the reference file name to a scratchpad buffer with no directory extension if the reference is for a directory
          if(refFileName[i] != '.' && refFileName[i] != '<' && refFileName[i] != '>'){
            refFileNameNoDir[i]=refFileName[i];
          }else{
            refFileNameNoDir[i]=0x00; //If the character is part of a directory extension, don't copy it
          } 
        }
      }else{
        for(int i=0; i<24; i++){
          refFileNameNoDir[i]=refFileName[i]; //Copy the reference directly to the scratchpad buffer if it's not a directory reference
        }
      }
    }

    directoryAppend(refFileNameNoDir);  //Add the reference to the directory buffer

    if(SD.exists(directory)){ //If the file or directory exists on the SD card...
      entry=SD.open(directory); //...open it...
      return_reference(); //send a refernce return to the TPDD port with its info...
      entry.close();  //...close the entry.
    }else{  //If the file does not exist...
      return_blank_reference();
    }

    upDirectory();  //Strip the reference off of the directory buffer
    
  }else if(searchForm == 0x01){ //Request first directory block
    root.close();
    root = SD.open(directory);
   ref_openFirst(); 
  }else if(searchForm == 0x02){ //Request next directory block
    root.close();
    root = SD.open(directory);
    ref_openNext();
  }else{  //Parameter is invalid
    return_normal(0x36);  //Send a normal return to the TPDD port with a parameter error
  }
}

void ref_openFirst(){
  directoryBlock = 0; //Set the current directory entry index to 0

  if(DME && directoryDepth>0 && directoryBlock==0){ //Return the "PARENT.<>" reference if we're in DME mode
    return_parent_reference();
  }else{
    ref_openNext();    //otherwise we just return the next reference
  }
}

void ref_openNext(){
  directoryBlock++; //Increment the directory entry index
  
  root.rewindDirectory(); //Pull back to the begining of the directory
  for(int i=0; i<directoryBlock-1; i++){  //skip to the current entry offset by the index
    root.openNextFile();
  }

  entry = root.openNextFile();  //Open the entry
  
  if(entry){  //If the entry exists it is returned
    if(entry.isDirectory() && !DME){  //If it's a directory and we're not in DME mode
      entry.close();  //the entry is skipped over
      ref_openNext(); //and this function is called again
    }
    
    return_reference(); //Send the reference info to the TPDD port
    entry.close();  //Close the entry
  }else{
    return_blank_reference();
  }
}

void command_open(){  //Opens an entry for reading, writing, or appending
  byte rMode = dataBuffer[(byte)(tail+4)];  //The access mode is stored in the 5th byte of the command
  entry.close();

  if(DME && strcmp(refFileNameNoDir, "PARENT") == 0){ //If DME mode is enabled and the reference is for the "PARENT" directory
    upDirectory();  //The top-most entry in the directory buffer is taken away
    directoryDepth--; //and the directory depth index is decremented
  }else{
    directoryAppend(refFileNameNoDir);  //Push the reference name onto the directory buffer
    if(DME && (int)strstr(refFileName, ".<>") != 0 && !SD.exists(directory)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
      SD.mkdir(directory);  //create the directory
      upDirectory();
    }else{
      entry=SD.open(directory); //Open the directory to reference the entry
      if(entry.isDirectory()){  //      !!!Moves into a sub-directory
        entry.close();  //If the entry is a directory
        directoryAppend("/"); //append a slash to the directory buffer
        directoryDepth++; //and increment the directory depth index
      }else{  //If the reference isn't a sub-directory, it's a file
        entry.close();
        switch(rMode){
          case 0x01: entry = SD.open(directory, FILE_WRITE); append=false; break; //Write
          case 0x02: entry = SD.open(directory, FILE_WRITE); append=true; break;  //Append, set the append flag
          case 0x03: entry = SD.open(directory, FILE_READ); append=false; break;  //Read
        }
        upDirectory();
      }
    }
  }
  
  if(SD.exists(directory)){ //If the file actually exists...
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
  int bytesRead = entry.read(fileBuffer, 0x80); //Try to pull 128 bytes from the file into the buffer
  //Serial.print("A: ");
  //Serial.println(entry.available(),HEX);

  if(bytesRead > 0){  //Send the read return if there is data to be read
    tpddWrite(0x10);  //Return type
    tpddWrite(bytesRead); //Data length
    for(int i=0; i<bytesRead; i++){
      tpddWrite(fileBuffer[i]);
    }
    tpddSendChecksum();
  }else{
    return_normal(0x3F);  //send a normal return with an end-of-file error if there is no data left to read
  }
}

void command_write(){ //Write a block of data from the command to the currently open entry
  byte commandDataLength = dataBuffer[(byte)(tail+3)];

  for(int i=0; i<commandDataLength; i++){
    if(append){
      entry.print(dataBuffer[(byte)(tail+4+i)]);  //If the append flag is set, use "print" to append to the file instead of "write"
    }else{
      entry.write(dataBuffer[(byte)(tail+4+i)]);
    }
  }
  
  return_normal(0x00);  //Send a normal return to the TPDD port with no error
}

void command_delete(){  //Delete the currently open entry
  entry.close();  //Close any open entries
  directoryAppend(refFileNameNoDir);  //Push the reference name onto the directory buffer
  entry = SD.open(directory, FILE_READ);  //directory can be deleted if opened "READ"
  
  if(DME && entry.isDirectory()){
    entry.rmdir();  //If we're in DME mode and the entry is a directory, delete it
  }else{
    entry.close();  //Files can be deleted if opened "WRITE", so it needs to be re-opened
    entry = SD.open(directory, FILE_WRITE);
    entry.remove();
  }
  
  upDirectory();
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

  directoryAppend(refFileNameNoDir);  //Push the current reference name onto the directory buffer
  
  if(entry){entry.close();} //Close any currently open entries
  entry = SD.open(directory); //Open the entry
  if(entry.isDirectory()){  //Append a slash to the end of the directory buffer if the reference is a sub-directory
    directoryAppend("/");
  }
  copyDirectory();  //Copy the directory buffer to the scratchpad directory buffer
  upDirectory();  //Strip the previous directory reference off of the directory buffer
  
  for(int i=4; i<28; i++){  //Loop through the command's data block, which contains the new entry name
      if(dataBuffer[(byte)(tail+i)]!=0x20 && dataBuffer[(byte)(tail+i)]!=0x00){ //If the current character is not a space (0x20) or null character...
        tempRefFileName[refIndex++]=dataBuffer[(byte)(tail+i)]; //...copy the character to the temporary reference name and increment the pointer.
      }
  }
  
  tempRefFileName[refIndex]=0x00; //Terminate the temporary reference name with a null character

  if(DME && entry.isDirectory()){ //      !!!If the entry is a directory, we need to strip the ".<>" off of the new directory name
    if(strstr(tempRefFileName, ".<>") != 0){
      for(int i=0; i<24; i++){
        if(tempRefFileName[i] == '.' || tempRefFileName[i] == '<' || tempRefFileName[i] == '>'){
          tempRefFileName[i]=0x00;
        } 
      }
    }
  }

  directoryAppend(tempRefFileName);
  if(entry.isDirectory()){
    directoryAppend("/");
  }

  //Serial.println(directory);
  //Serial.println(tempDirectory);
  SD.rename(tempDirectory,directory);  //Rename the entry

  upDirectory();

  entry.close();
  
  return_normal(0x00);  //Send a normal return to the TPDD port with no error
}

/*
 * 
 * TS-DOS DME Commands
 * 
 */

void command_DMEReq(){  //Send the DME return with the root directory's name
  if(DME){
    tpddWrite(0x12);
    tpddWrite(0x0B);
    tpddWriteString(" SDTPDD.<> "); //Name must be 12 characters long and be space character padded
    tpddSendChecksum();
  }else{
    return_normal(0x36);
  }
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
        }else if(dataBuffer[tail]=='M' && dataBuffer[(byte)(tail+1)]=='1'){ //If a DME command is received
          DME = true; //set the DME mode flag to true
          tail=tail+2;  //and skip past the command to the DME request command
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

  //Serial.print(tail,HEX); //Debug code that displays the tail index in the buffer where the command was found...
  //Serial.print("=");
  //Serial.print("T:"); //...the command type...
  //Serial.print(rType, HEX);
  //Serial.print("|L:");  //...and the command length.
  //Serial.print(rLength, HEX);
  //Serial.println(DME?'D':'.');
  
  switch(rType){  //Select the command handler routine to jump to based on the command type
    case 0x00: command_reference(); break;
    case 0x01: command_open(); break;
    case 0x02: command_close(); break;
    case 0x03: command_read(); break;
    case 0x04: command_write(); break;
    case 0x05: command_delete(); break;
    case 0x06: command_format(); break;
    case 0x07: command_status(); break;
    case 0x08: command_DMEReq(); break; //DME Command
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
