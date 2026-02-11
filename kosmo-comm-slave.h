#ifndef KosmoCommSlave_h
#define KosmoCommSlave_h

#include <Wire.h>
#include "Definitions.h"
#include "Channel.h"

#define SLAVE_ADDR 9
#define MASTER_REQUEST_TIMEOUT 2000
#define MAX_PARTS 8


bool newPartData = false;
DrumSequencer registers;
DrumSequencer nextRegisters;

DrumSequencer parts[MAX_PARTS];
int currentPartIndex = -1;
int partDataIndex = 0;

unsigned long lastMasterRequest = 0;
unsigned long lastMasterReceive = 0;
const size_t totalSize = sizeof(DrumSequencer);
int currentRequestChunk = 0;
int currentReceiveChunk = 0;
int totalChunks = (totalSize + 31) / 32;
uint8_t receiveBuffer[totalSize] = {0};

void updateRegisterPageSteps(int channelIndex, int pageNumber, uint16_t steps) {
 registers.channel[channelIndex].page[pageNumber] = steps;
}

void updateRegisterLastStep(int channelIndex, int lastStep) {
  registers.channel[channelIndex].lastStep = lastStep;
}

void updateRegisterDivider(int channelIndex, int divider) {
  registers.channel[channelIndex].divider = divider;
}

void updateRegisterChannelEnabled(int channelIndex, bool enabled) {
  registers.channel[channelIndex].enabled = enabled;  
}

void onRequest() {
  lastMasterRequest = millis();

  char s[100];
  sprintf(s, "Sending %d/%d => ", currentRequestChunk, totalChunks);
  Serial.print(s);

  if(currentRequestChunk < totalChunks) {
    size_t offset = currentRequestChunk * 32;
    size_t chunkSize = min(32, totalSize-offset);
    uint8_t buffer[chunkSize];
    memcpy(buffer, (uint8_t*)&registers + offset, chunkSize);  
    for(int j=0; j<chunkSize; j++) {
      Serial.print(buffer[j], HEX);
      Serial.print(" ");
    }
    Serial.println();

    Wire.write(buffer, chunkSize); // send data
    currentRequestChunk++;
  }
}

void receivePartIndex() {
  if(Wire.available()) {
    currentPartIndex = Wire.read();
    if(currentPartIndex >= 0 && currentPartIndex < MAX_PARTS) {
      nextRegisters = parts[currentPartIndex];

      Serial.print("part ");
      Serial.print(currentPartIndex);
      Serial.print(" => ");

      //printDrumSequencer(nextRegisters);

      newPartData = true;
    }
  }
}

void receivePartData() {
  size_t offset = currentReceiveChunk * 32;  
  size_t bytesRead = 0;

  if(currentReceiveChunk < totalChunks) {
    
    while(Wire.available()) {
      char data = Wire.read();
      receiveBuffer[offset + bytesRead] = data;
      bytesRead++;
    }

    // for(int j=0; j<bytesRead; j++) {
    //   Serial.print(receiveBuffer[offset + j], HEX);
    //   Serial.print(" ");
    // } 
    // Serial.println();
  
    currentReceiveChunk++;
  }

  if(currentReceiveChunk == totalChunks) {
    if(partDataIndex == MAX_PARTS)
      partDataIndex = 0;

    // we are done - copy from buffer into "nextRegister"
    //memcpy((uint8_t*)&nextRegisters, receiveBuffer, totalSize);  
    memcpy((uint8_t*)&parts[partDataIndex], receiveBuffer, totalSize);  
    currentReceiveChunk = 0;
    lastMasterReceive = 0;


    Serial.print("part ");
    Serial.print(partDataIndex);
    Serial.print(" => ");

    printDrumSequencer(parts[partDataIndex]);

    partDataIndex++;


  }
}

void onReceive(int size) {
  /*
  * size = 1 => setting a 1 digit part index (0-9)
  * size = 2 => setting a 2 digit part index (10-15) <= LATER
  * size > 2 => setting part data
  */
  lastMasterReceive = millis();

  if(size == 1) {
    receivePartIndex();
  } else {
    receivePartData();
  }
}

void initializeParts() {
  for (int i = 0; i < MAX_PARTS; i++) {
    for (int j = 0; j < 5; j++) {
      for (int k = 0; k < 4; k++) {
        parts[i].channel[j].page[k] = 0; // Initialize page to 0
      }
      parts[i].channel[j].divider = 6;     // Default divider
      parts[i].channel[j].lastStep = 15;   // Default last step
      parts[i].channel[j].enabled = true;  // Default enabled
    }
    parts[i].chainModeEnabled = false;     // Default chain mode
  }
}

void setupSlave(Channel channels[CHANNELS]) {
  Wire.begin(SLAVE_ADDR);
  Wire.setClock(400000);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  initializeParts();

  Serial.println("kosmo slave ready");  
}


void resetSlaveRequestBuffer() {
  currentRequestChunk = 0;
  lastMasterRequest = 0;
  partDataIndex = 0;
  Serial.println("Reset Slave Request Buffer");
}




#endif