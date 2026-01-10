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
  uint16_t page1;
  uint16_t page2;
  uint16_t page3;
  uint16_t page4;
  int divider;
  int lastStep;
  bool enabled;
};

struct DrumSequencerRegisters {
  bool chainModeEnabled;

  uint16_t ch1_page1;
  uint16_t ch1_page2;
  uint16_t ch1_page3;
  uint16_t ch1_page4;  
  int ch1_divider;
  int ch1_lastStep;
  bool ch1_enabled;  

  uint16_t ch2_page1;
  uint16_t ch2_page2;
  uint16_t ch2_page3;
  uint16_t ch2_page4;  
  int ch2_divider;
  int ch2_lastStep;
  bool ch2_enabled;    

  uint16_t ch3_page1;
  uint16_t ch3_page2;
  uint16_t ch3_page3;
  uint16_t ch3_page4;  
  int ch3_divider;
  int ch3_lastStep;
  bool ch3_enabled;    

  uint16_t ch4_page1;
  uint16_t ch4_page2;
  uint16_t ch4_page3;
  uint16_t ch4_page4;  
  int ch4_divider;
  int ch4_lastStep;
  bool ch4_enabled;    

  uint16_t ch5_page1;
  uint16_t ch5_page2;
  uint16_t ch5_page3;
  uint16_t ch5_page4;  
  int ch5_divider;
  int ch5_lastStep;
  bool ch5_enabled;          
};

void printDrumSequencerRegisters(DrumSequencerRegisters reg) {
  char s[100];
  sprintf(s, "ch1 => laststep: %d | divider: %d | output enabled: ", reg.ch1_lastStep, reg.ch1_divider);
  Serial.print(s);
  Serial.println(reg.ch1_enabled);
  Serial.print("steps: ");
  printInt(reg.ch1_page1);
  printInt(reg.ch1_page2);
  printInt(reg.ch1_page3);
  printIntln(reg.ch1_page4);      

  sprintf(s, "ch2 => laststep: %d | divider: %d | output enabled: ", reg.ch2_lastStep, reg.ch2_divider);
  Serial.print(s);
  Serial.println(reg.ch2_enabled);
  Serial.print("steps: ");
  printInt(reg.ch2_page1);
  printInt(reg.ch2_page2);
  printInt(reg.ch2_page3);
  printIntln(reg.ch2_page4);

  sprintf(s, "ch3 => laststep: %d | divider: %d | output enabled: ", reg.ch3_lastStep, reg.ch3_divider);
  Serial.print(s);
  Serial.println(reg.ch3_enabled);
  Serial.print("steps: ");
  printInt(reg.ch3_page1);
  printInt(reg.ch3_page2);
  printInt(reg.ch3_page3);
  printIntln(reg.ch3_page4);    

  sprintf(s, "ch4 => laststep: %d | divider: %d | output enabled: ", reg.ch4_lastStep, reg.ch4_divider);
  Serial.print(s);
  Serial.println(reg.ch4_enabled);
  Serial.print("steps: ");
  printInt(reg.ch4_page1);
  printInt(reg.ch4_page2);
  printInt(reg.ch4_page3);
  printIntln(reg.ch4_page4);    

  sprintf(s, "ch5 => laststep: %d | divider: %d | output enabled: ", reg.ch5_lastStep, reg.ch5_divider);
  Serial.print(s);
  Serial.println(reg.ch5_enabled);
  Serial.print("steps: ");
  printInt(reg.ch5_page1);
  printInt(reg.ch5_page2);
  printInt(reg.ch5_page3);
  printIntln(reg.ch5_page4);              
}

#endif