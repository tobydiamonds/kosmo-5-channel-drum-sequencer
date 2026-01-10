#ifndef KosmoCommSlave_h
#define KosmoCommSlave_h

#include <Wire.h>
#include "Definitions.h"
#include "Channel.h"

#define SLAVE_ADDR 9

bool programming = false;
bool readyToSendRegisters = false;
bool newPartData = false;

DrumSequencerRegisters registers;
DrumSequencerRegisters nextRegisters;

String command = "";
bool setFromMaster = false;
int chunkIndex = -1;
size_t remaningBytes = 0;

void updateRegisterPageSteps(int channelIndex, int pageNumber, uint16_t steps) {
  if(channelIndex==0) {
    if(pageNumber==0) registers.ch1_page1 = steps;
    else if(pageNumber==1) registers.ch1_page2 = steps;
    else if(pageNumber==2) registers.ch1_page3 = steps;
    else if(pageNumber==3) registers.ch1_page4 = steps;
  } else   if(channelIndex==1) {
    if(pageNumber==0) registers.ch2_page1 = steps;
    else if(pageNumber==1) registers.ch2_page2 = steps;
    else if(pageNumber==2) registers.ch2_page3 = steps;
    else if(pageNumber==3) registers.ch2_page4 = steps;
  } else   if(channelIndex==2) {
    if(pageNumber==0) registers.ch3_page1 = steps;
    else if(pageNumber==1) registers.ch3_page2 = steps;
    else if(pageNumber==2) registers.ch3_page3 = steps;
    else if(pageNumber==3) registers.ch3_page4 = steps;
  } else   if(channelIndex==3) {
    if(pageNumber==0) registers.ch4_page1 = steps;
    else if(pageNumber==1) registers.ch4_page2 = steps;
    else if(pageNumber==2) registers.ch4_page3 = steps;
    else if(pageNumber==3) registers.ch4_page4 = steps;
  } else  if(channelIndex==4) {
    if(pageNumber==0) registers.ch5_page1 = steps;
    else if(pageNumber==1) registers.ch5_page2 = steps;
    else if(pageNumber==2) registers.ch5_page3 = steps;
    else if(pageNumber==3) registers.ch5_page4 = steps;
  } 
}

void updateRegisterLastStep(int channelIndex, int lastStep) {
  switch(channelIndex) {
    case 0:
      registers.ch1_lastStep = lastStep;
      break;
    case 1:
      registers.ch2_lastStep = lastStep;
      break;
    case 2:
      registers.ch3_lastStep = lastStep;
      break;
    case 3:
      registers.ch4_lastStep = lastStep;
      break;
    case 4:
      registers.ch5_lastStep = lastStep;
      break;                        
  }
}

void updateRegisterDivider(int channelIndex, int divider) {
  switch(channelIndex) {
    case 0:
      registers.ch1_divider = divider;
      break;
    case 1:
      registers.ch2_divider = divider;
      break;
    case 2:
      registers.ch3_divider = divider;
      break;
    case 3:
      registers.ch4_divider = divider;
      break;
    case 4:
      registers.ch5_divider = divider;
      break;                        
  }
}

void updateRegisterChannelEnabled(int channelIndex, bool enabled) {
  switch(channelIndex) {
    case 0:
      registers.ch1_enabled = enabled;
      break;
    case 1:
      registers.ch2_enabled = enabled;
      break;
    case 2:
      registers.ch3_enabled = enabled;
      break;
    case 3:
      registers.ch4_enabled = enabled;
      break;
    case 4:
      registers.ch5_enabled = enabled;
      break;                        
  }
}

bool receivePartData() {
  uint8_t buffer[32];
  size_t bytesRead = 0;

  while(bytesRead < min(remaningBytes, 32)) {
    if(Wire.available()) {
      char data = Wire.read();
      buffer[bytesRead++] = data;
    }
  }

  size_t offset = chunkIndex * 32;
  memcpy((uint8_t*)&nextRegisters + offset, buffer, bytesRead);  
  chunkIndex++;
  remaningBytes -= bytesRead;
  return remaningBytes > 0;
}

void receiveCommand(int size) {

  if(command == "set") {
    setFromMaster = receivePartData();
  }

  if(!setFromMaster) {

    command = "";
    while (Wire.available()) {
      char c = Wire.read();
      command += c;
    }
    chunkIndex = -1;

    Serial.print("Command from master - command: ");
    Serial.println(command);

    if (command == "prg") {
      // programming = true;
      // readyToSendRegisters = false;
      Serial.println("Programming started");
    } else if (command == "end") {
      // programming = false;
      // readyToSendRegisters = false;
      Serial.println("Programming ended");
    } else if (command.startsWith("get") && command.length() == 5) {
      chunkIndex = command.substring(4).toInt();
      command = "get";
    } else if (command.startsWith("set") && command.length() == 5) {
      chunkIndex = command.substring(4).toInt();
      command = "set";
      setFromMaster = true;
      remaningBytes = sizeof(DrumSequencerRegisters);
      //Serial.println("SET started");
    } else if (command == "endset") {
      //Serial.println("SET ended");
      printDrumSequencerRegisters(nextRegisters);      
      // update channels with data from register
      newPartData = true;
    }
  }
}



void onRequestOLD() {
  if (command == "get" && chunkIndex >= 0) {
    readyToSendRegisters = false; // Reset flag after sending

    const size_t totalSize = sizeof(DrumSequencerRegisters);
    uint8_t buffer[totalSize];
    memcpy(buffer, &registers, totalSize);

    size_t totalChunks = (totalSize + 30) / 31;
    if(chunkIndex < totalChunks) {
      size_t offset = chunkIndex * 31;
      size_t chunkSize = min(31, totalSize-offset);
      bool more = chunkIndex < (totalChunks - 1);
      uint8_t status = 1;
      if(more)
        status |= 0x80;
      Wire.write(status); // send status
      Wire.write(&buffer[offset], chunkSize); // send data
      readyToSendRegisters = more;
    }
  }  else {
    Wire.write(0); // send status
  }
}

void onRequest() {
  
}



void setupSlave(Channel channels[CHANNELS]) {
  registers.chainModeEnabled = false;
  for(int i=0; i<CHANNELS; i++) {
    for(int p=0; p<4; p++) {
      updateRegisterPageSteps(i, p, channels[i].GetStepsByPage(p, false));
    }
  }
  registers.ch1_divider = 6;
  registers.ch1_lastStep = 15;
  registers.ch1_enabled = false;
  registers.ch2_divider = 6;
  registers.ch2_lastStep = 15;
  registers.ch2_enabled = false;
  registers.ch3_divider = 6;
  registers.ch3_lastStep = 15;
  registers.ch3_enabled = false;
  registers.ch4_divider = 6;
  registers.ch4_lastStep = 15;
  registers.ch4_enabled = false;
  registers.ch5_divider = 6;
  registers.ch5_lastStep = 15;
  registers.ch5_enabled = false;                
  


  Wire.begin(SLAVE_ADDR);
  Wire.setClock(400000);
  Wire.onReceive(receiveCommand);
  Wire.onRequest(onRequest);
  Serial.println("kosmo slave ready");
}



#endif