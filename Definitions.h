#ifndef Definitions_h
#define Definitions_h

#define PPQN 24.0
#define MAX_PAGE 4
#define STEPS_PR_PAGE 16
#define MAX_STEPS 64
#define CHANNELS 5

#define OUTPUT_PULSE 20
#define LED_SHORT_PULSE 50
#define LED_VERY_SHORT_PULSE 25



uint8_t MapToByte(uint16_t value, uint8_t lower, uint8_t upper) {
  uint8_t result = map(value, 0, 1023, lower, upper);
  if(value >= 1000)
    result = upper;
  return result;
}

void printByte(uint8_t b, uint8_t bitOrder = LSBFIRST) {
  if(bitOrder == LSBFIRST) {
    for(int j=7; j>=0; j--) {
      if((b >> j) & 1)
        Serial.print("1");
      else
        Serial.print("0");
    }    
  } else {
    for(int j=0; j<8; j++) {
      if((b >> j) & 1)
        Serial.print("1");
      else
        Serial.print("0");
    }        
  }
  Serial.print(" ");
}

void printByteln(uint8_t b, uint8_t bitOrder = LSBFIRST) {
  printByte(b, bitOrder);
  Serial.println();
}

void printInt(uint16_t value) {
  for(int i=0; i<16; i++) {
    if((value >> i) & 1) Serial.print("1");
    else Serial.print("0");
  }
  Serial.print(" ");
}

void printIntln(uint16_t value) {
  printInt(value);
  Serial.println();
}


struct DrumSequencerChannel {
  uint16_t page[4] = {0};
  int divider = 6;
  int lastStep = 15;
  bool enabled = true;  
};

struct DrumSequencer {
  DrumSequencerChannel channel[5];
  bool chainModeEnabled = false;  
};

void printDrumSequencerChannel(DrumSequencerChannel channel, int index) {
  char s[100];
  sprintf(s, "ch%d => laststep: %d | divider: %d | output enabled: ", index, channel.lastStep, channel.divider);
  Serial.print(s);
  Serial.println(channel.enabled);
  Serial.print("steps: ");  
  for(int i=0; i<4; i++) {
    printInt(channel.page[i]);
  }
  Serial.println();
}

void printDrumSequencer(DrumSequencer drums) {
  for(int i=0; i<5; i++) {
    printDrumSequencerChannel(drums.channel[i], i);
  }
  Serial.print("chain mode enabled: ");
  Serial.println(drums.chainModeEnabled);
}


#endif