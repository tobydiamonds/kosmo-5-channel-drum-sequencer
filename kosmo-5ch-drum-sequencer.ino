#include <Button.h>
#include "Definitions.h"
#include "AnalogInput.h"
#include "Channel.h"
#include "kosmo-comm-slave.h"

#define CLOCK_IN_PIN 2
#define RESET_IN_PIN 3
#define PAGE_BUTTON_PIN 13
#define CLOCK_OUT_PIN 52

// channel leds
/*
* J2 . 4 pin => output/74HC595
*    74HC595		UNO
* 1  14/SER		  28
* 2  12/RCLK		29
* 3  11/SRCLK		30
* 4  9/QH			  nc
*/
#define CHANNELS_LED_DATA_PIN 28
#define CHANNELS_LED_LATCH_PIN 29
#define CHANNELS_LED_CLOCK_PIN 30

// channel switches
/*
*
* J3 - 5 pin => inputs/74HC165
*    74HC165		UNO
* 1  1/SH/LD		22
* 2  2/CLK		  23
* 3  9/QH			  24
* 4  10/SER		  nc
* 5  15/CLK IN	26
*/
#define CHANNELS_SWITCH_LOAD_PIN 22
#define CHANNELS_SWITCH_CLOCK_PIN 23
#define CHANNELS_SWITCH_DATA_PIN 24
#define CHANNELS_SWITCH_FREEZE_PIN 26


#define INPUT_SCAN_INTERVAL 50
#define PAGE_BUTTON_SHORTPRESS_INTERVAL 500
#define MANUAL_OVERRIDE_INTERVAL 5000

bool clockInLed = false;
bool clockOutLed = false;
volatile bool edgeDetected = false;
volatile bool reset = false;
volatile bool hasPulse = false;

uint8_t currentPage = 0;
uint8_t currentStep = 0;
uint8_t allChannelsLastStep = 0;
uint16_t currentPageSteps[CHANNELS] = {0};

int bpm120NoteInterval = 500; // 500ms interval between quarter notes at 120bpm
float pulseInterval = bpm120NoteInterval / PPQN;


unsigned long now = 0;
unsigned long lastScanInterval = 0;
unsigned long lastClockInLed = 0;
unsigned long lastClockOutLed = 0;
unsigned long lastClockPulse = 0;
unsigned long quaterNodeInterval = 0;
unsigned long lastQuaterNode = 0;

Button pageButton(PAGE_BUTTON_PIN);

bool channelSetLastStepMode = false;
bool shortPress = false;
bool firstScanAfterPageChange = true;
bool manualOverride = false;
bool chainMode = false;

Channel channels[CHANNELS] = {Channel(0, 42, A4), Channel(1, 44, A3), Channel(2, 46, A2), Channel(3, 48, A1), Channel(4, 50, A0)};
uint16_t uiSwitchSnapshot[CHANNELS] = {0};
bool uiOutputEnabledSwitchSnapshot[CHANNELS] = {false};

void setup() {
  Serial.begin(115200);

  // channels switches setup
  pinMode(CHANNELS_SWITCH_LOAD_PIN, OUTPUT);
  pinMode(CHANNELS_SWITCH_CLOCK_PIN, OUTPUT);
  pinMode(CHANNELS_SWITCH_FREEZE_PIN, OUTPUT);
  pinMode(CHANNELS_SWITCH_DATA_PIN, INPUT);

  // channels leds setup
  pinMode(CHANNELS_LED_DATA_PIN, OUTPUT);
  pinMode(CHANNELS_LED_LATCH_PIN, OUTPUT);
  pinMode(CHANNELS_LED_CLOCK_PIN, OUTPUT);


  pinMode(PAGE_BUTTON_PIN, INPUT);

  // clock in
  pinMode(CLOCK_IN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CLOCK_IN_PIN), onClockPulse, RISING);

  // reset in
  pinMode(RESET_IN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RESET_IN_PIN), onReset, RISING);

  // clock out
  pinMode(CLOCK_OUT_PIN, OUTPUT);
  digitalWrite(CLOCK_OUT_PIN, LOW);


  pageButton.begin();

  Serial.print("pulse interval: ");
  Serial.println(pulseInterval);

  
  for(int i=0; i<CHANNELS; i++) {
    channels[i].Begin();
    channels[i].OnLastStepSet(onLastStepSet);
    channels[i].OnTempoDividerSet(onTempoDividerSet);
  }

  allChannelsLastStep = GetAllChannelsLastStep();    

  // i2c comm
  setupSlave(channels);

  Serial.println("Drum Sequencer ready");
}

uint16_t GetAllChannelsLastStep() {
  uint8_t max = 0;

  for(int i=0; i<CHANNELS; i++) {
    if(channels[i].LastStep() > max)
      max = channels[i].LastStep();
  }
  return max;
}

void onLastStepSet(uint8_t channelNumber, uint8_t lastStep) {
  allChannelsLastStep = GetAllChannelsLastStep();
  updateRegisterLastStep(channelNumber, lastStep);
}

void onTempoDividerSet(uint8_t channelNumber, int divider) {
  updateRegisterDivider(channelNumber, divider);
}

uint8_t read165byte() {
  uint8_t value = 0;
  for (int i = 0; i < 8; i++) {
    digitalWrite(CHANNELS_SWITCH_CLOCK_PIN, LOW);      // prepare falling edge
    if (digitalRead(CHANNELS_SWITCH_DATA_PIN)) {
      value |= (1 << i);               // store bit in LSB first
    }
    digitalWrite(CHANNELS_SWITCH_CLOCK_PIN, HIGH);     // shift register updates here
  }
  return value;
}

void scanOutputBoard() {
  /*
  * 2 74HC165 where board 1s 5 first inputs are channel 0-4 enable and 6th input is clock-out/chaing toggle
  * only the first 165 is used
  */
  uint8_t value = ~read165byte();
  read165byte();

  for(int i=0; i<CHANNELS; i++) {
    uint8_t v = value;
    int index = 7-i;
    bool state = (v >> index) & 1;

    if(state != uiOutputEnabledSwitchSnapshot[i]) {
      channels[i].SetEnabled(state);
      updateRegisterChannelEnabled(i, state);
      uiOutputEnabledSwitchSnapshot[i] = state;
    }
  }

  chainMode = (value & 0x04) == 0x04;
}

void scanInputs() {
    // load shift register
    digitalWrite(CHANNELS_SWITCH_LOAD_PIN, LOW);
    //delayMicroseconds(5);
    digitalWrite(CHANNELS_SWITCH_LOAD_PIN, HIGH);
    //delayMicroseconds(5);

    digitalWrite(CHANNELS_SWITCH_CLOCK_PIN, HIGH);
    digitalWrite(CHANNELS_SWITCH_FREEZE_PIN, LOW);

    // scan output board
    scanOutputBoard(); // this will perform 2x reads from 165-chain

    // scan channels
    for (int i = 0; i < CHANNELS; i++) {
        uint8_t incoming1 = read165byte();//shiftIn(CHANNELS_SWITCH_DATA_PIN, CHANNELS_SWITCH_CLOCK_PIN, LSBFIRST);
        uint8_t incoming2 = read165byte();//shiftIn(CHANNELS_SWITCH_DATA_PIN, CHANNELS_SWITCH_CLOCK_PIN, LSBFIRST);

        uint16_t currentUI = ((uint16_t)incoming1 << 8) | incoming2;
        currentUI = ~currentUI; // active-low fix
        
        uint8_t channelLastPage = channels[i].LastStep() / 16;

        if (firstScanAfterPageChange) {
            uiSwitchSnapshot[i] = currentUI; // record UI state when entering page
        } else {
          // Detect which switches changed since entering the page
          uint16_t changedBits = currentUI ^ uiSwitchSnapshot[i];
          
          if (changedBits != 0 && channelLastPage >= currentPage) {
              // Get current steps for the page
              uint16_t pageSteps = channels[i].GetStepsByPage(currentPage, false);

              // Apply only changed switches
              pageSteps ^= (changedBits & currentUI); // turn ON changed-on bits
              pageSteps &= ~(changedBits & ~currentUI); // turn OFF changed-off bits

              // Store back
              channels[i].SetSteps(currentPage, pageSteps);
              updateRegisterPageSteps(i, currentPage, pageSteps); // update registers for i2c comm

              // Update snapshot so we don’t re-trigger until another change
              uiSwitchSnapshot[i] = currentUI;
          }
        }

        // update the UI according to current page
        if(channelLastPage >= currentPage) {
          currentPageSteps[i] = channels[i].GetStepsByPage(currentPage, hasPulse && channels[i].Enabled());
        } else {
          currentPageSteps[i] = 0;
        }
    }

    if (firstScanAfterPageChange)
        firstScanAfterPageChange = false;

    digitalWrite(CHANNELS_SWITCH_FREEZE_PIN, HIGH);
}

void outputByte(uint8_t b, uint8_t bitOrder = LSBFIRST) {
    shiftOut(CHANNELS_LED_DATA_PIN, CHANNELS_LED_CLOCK_PIN, bitOrder, b);

    //printByte(b, bitOrder);
}

void updateChannelStepLeds() {
  for(int i=0; i<CHANNELS; i++) {
    uint16_t steps = currentPageSteps[i];//channels[i].GetCurrentPattern();
    uint8_t highByte = (steps >> 8) & 0xFF;
    uint8_t lowByte  = steps & 0xFF;
    outputByte(lowByte);
    outputByte(highByte);
  }
}

void updateChannelLastStepLeds() {
  // set leds for each bit in the last channel step value
  for(int i=0; i<CHANNELS; i++) {
    uint8_t value = channels[i].LastStep();
    while(value > STEPS_PR_PAGE)
      value = value - STEPS_PR_PAGE;

    value = 15 - value;
    uint16_t bits = (1 << value);
    uint8_t msb = (bits >> 8) & 0xFF;
    uint8_t lsb = bits & 0xFF;

    outputByte(lsb);
    outputByte(msb);
  }

} 

void updateOutputBoard() {
  /*
  * enable & output leds
  *
  * second byte (first 595)             first byte (second 595)
  * bit 0/0x01: channel 1 enabled led   bit 0: channel 1 output led
  * bit 1/0x02: channel 2 enabled led   bit 1: channel 2 output led
  * bit 2/0x04: channel 3 enabled led   bit 2: channel 3 output led
  * bit 3/0x08: channel 4 enabled led   bit 3: channel 4 output led
  * bit 4/0x10: channel 5 enabled led   bit 4: channel 5 output led
  * bit 5/0x20: clock out led           bit 5: nc 
  * bit 6/0x40: chain-mode led          bit 6: nc
  * bit 7/0x80: clock-mode led          bit 7: nc
  *
  */

  uint8_t firstByte = 0x00;
  uint8_t secondByte = 0x00;
  for(int i=0; i<CHANNELS; i++) {
    if(channels[i].OutputLedState()) {
      firstByte |= (1 << i);
    }
    if(channels[i].Enabled()) {
      secondByte |= (1 << i);
    }
  }

  if(clockOutLed)
    secondByte |= 0x20;
  else
    secondByte &= 0xDF;

  if(chainMode)
    secondByte |= 0x40;
  else
    secondByte |= 0x80;
  outputByte(firstByte, MSBFIRST);
  outputByte(secondByte, MSBFIRST);
}

void updateDividerBoard() {
  /*
    bit 0-4: divider leds
    bit 5-8: page leds
    bit 9:   clock in led 
  */

  uint8_t firstByte = 0x00;
  uint8_t secondByte = 0x00;

  if(currentPage==0)
    secondByte |= 0x20;
  else if(currentPage==1)
    secondByte |= 0x40;
  else if(currentPage==2)
    secondByte |= 0x80;
  else if(currentPage==3)
    firstByte |= 0x01;
  
  for(int i=0; i<CHANNELS; i++) {
    if(channels[i].DividerLedState()) {
      secondByte |= (1 << i);
    }
  }

  if(clockInLed)
    firstByte |= 0x02;
  else
    firstByte &= 0xFD;  

  outputByte(firstByte, MSBFIRST);
  outputByte(secondByte, MSBFIRST);  
}

void UpdateTriggerBoard() {
  uint8_t firstByte = 0x00;
  uint8_t secondByte = 0x00;

  if(currentStep >= 8)
    firstByte = (1 << currentStep-8);
  else
    secondByte = (1 << currentStep);

  outputByte(firstByte, MSBFIRST);
  outputByte(secondByte, MSBFIRST);    
}

void updateUI() {
  digitalWrite(CHANNELS_LED_LATCH_PIN, LOW);

  UpdateTriggerBoard();
  updateDividerBoard();
  updateOutputBoard();

  if(channelSetLastStepMode)
    updateChannelLastStepLeds();
  else
    updateChannelStepLeds();

  digitalWrite(CHANNELS_LED_LATCH_PIN, HIGH);
}

int ppqnCounter = 0;
int oldStep = -1;
void triggerClockPulse() {
  ppqnCounter = (ppqnCounter + 1) % 24;
  for(int i=0; i<CHANNELS; i++)
    channels[i].Pulse(ppqnCounter, now);

  if(oldStep != currentStep) {
    oldStep = currentStep;
    triggerStep();
  }

  if (ppqnCounter % 6 == 0) {
    if(currentStep < allChannelsLastStep)
      currentStep++;
    else
      currentStep = 0;


  }
}

void triggerStep() {
  if(currentStep % 4 == 0) {
    clockInLed = true;
    lastClockInLed = now;

    // todo: handle chain out mode, where clock out stops after 1 pattern
    clockOutLed = true; // turn on clock out led
    lastClockOutLed = now;

    quaterNodeInterval = now - lastQuaterNode;
    lastQuaterNode = now;    
  }
}

void onClockPulse() {
  lastClockPulse = now;  
  edgeDetected = true;
  hasPulse = true;
  digitalWrite(CLOCK_OUT_PIN, HIGH);
}

void onReset() {
  reset = true;
}

unsigned long buttonPressTime = 0;
bool buttonPressDetected = false;

void loop() {
  
  now = millis();

  // // simulate clock in
  // if(now > (lastClockPulse + pulseInterval)) {
  //   onClockPulse();
  // }
  // handle reset
  if(now > (lastClockPulse + 2000) && hasPulse) {
    reset = true;
    hasPulse = false;
  }  

  // handle reset
  if(reset) {
    reset = false;
    currentPage = 0;
    currentStep = 0;
    ppqnCounter = 0;
    oldStep = -1;    
    firstScanAfterPageChange = true;
    hasPulse = false;
    lastQuaterNode = 0;
    lastClockPulse = 0;
    lastClockInLed = 0;
    lastClockOutLed = 0;
    edgeDetected = false;
    for(int i=0; i<CHANNELS; i++)
      channels[i].Reset();
    Serial.println("reset!");
  }

  // load new data into drum channels?
  if(newPartData && (!hasPulse || currentStep == GetAllChannelsLastStep())) { // last step in current part
    newPartData = false;
    //Serial.println("Next part playing");

    for(int i=0; i<CHANNELS; i++) {
      channels[i].LoadPartData(nextRegisters);
      channels[i].Reset();

      registers = nextRegisters;
    }
  } 

  // handle clock in
  if(edgeDetected) {
    edgeDetected = false;
    triggerClockPulse();
  }
  
  if(now > (lastClockPulse + 2)) {
    digitalWrite(CLOCK_OUT_PIN, LOW);
  }



  for(int i=0; i<CHANNELS; i++) {
    channels[i].Run(now, channelSetLastStepMode, currentPage);
  }



  if(pageButton.released()) {
    if(shortPress) { // change page
      if(currentPage < MAX_PAGE-1)
        currentPage++;
      else
        currentPage = 0;

      firstScanAfterPageChange = true;
      Serial.print("Current page: ");
      Serial.println(currentPage);
    } else if (channelSetLastStepMode) {
      for(int i=0; i<CHANNELS; i++) {
        channels[i].StartSetDivider();
      }      
    }

    channelSetLastStepMode = false;
    buttonPressDetected = false;
    buttonPressTime = 0;
    manualOverride = false;
  }  

  if(pageButton.pressed()) {
    if(!buttonPressDetected) {
      buttonPressDetected = true;
      buttonPressTime = now;
      manualOverride = true;
    }
  }

  shortPress = (buttonPressTime + PAGE_BUTTON_SHORTPRESS_INTERVAL) > now;
  if(buttonPressDetected && !shortPress && !channelSetLastStepMode) {
    channelSetLastStepMode = true;
    for(int i=0; i<CHANNELS; i++) {
      channels[i].StartSetLastStep();
    }
  }



  if(now >(lastClockInLed + LED_SHORT_PULSE)) {
    clockInLed = false;
  }

  if (now > (lastClockOutLed + LED_SHORT_PULSE)) {
    clockOutLed = false;
  }  

  if(now > (lastScanInterval + INPUT_SCAN_INTERVAL)) {
    lastScanInterval = now;
    scanInputs();
    updateUI();
  }

 

  if(currentRequestChunk != 0 && now > (lastMasterRequest + MASTER_REQUEST_TIMEOUT)) {
    resetSlaveRequestBuffer();
  }
}
