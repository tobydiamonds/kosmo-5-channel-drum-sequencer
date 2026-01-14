#ifndef KosmoCommSlave_h
#define KosmoCommSlave_h

#include <Wire.h>
#include "Definitions.h"
#include "Channel.h"

#define SLAVE_ADDR 9
#define MASTER_REQUEST_TIMEOUT 2000


bool newPartData = false;
DrumSequencer registers;
DrumSequencer nextRegisters;

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

void onReceive(int size) {
  lastMasterReceive = millis();

  // char s[100];
  // sprintf(s, "Receiving %d/%d (%d bytes) => ", currentReceiveChunk, totalChunks, size);
  // Serial.print(s);  

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
    // we are done - copy from buffer into "nextRegister"
    memcpy((uint8_t*)&nextRegisters, receiveBuffer, totalSize);  
    currentReceiveChunk = 0;
    lastMasterReceive = 0;

    //printDrumSequencer(nextRegisters);

    newPartData = true;
  }
}

void setupSlave(Channel channels[CHANNELS]) {
  Wire.begin(SLAVE_ADDR);
  Wire.setClock(400000);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
  Serial.println("kosmo slave ready");  
}


void resetSlaveRequestBuffer() {
  currentRequestChunk = 0;
  lastMasterRequest = 0;
  Serial.println("Reset Slave Request Buffer");
}




#endif