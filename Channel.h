#ifndef Channel_h
#define Channel_h

#include "Definitions.h"
#include "AnalogInput.h"

typedef void (*LastStepSet)(uint8_t, uint8_t); // channel, last step
typedef void (*TempoDividerSet)(uint8_t, int); // channel, last step


class Channel {

private:
  uint8_t _channelNumber;
  bool _enabled;
  uint16_t _steps[MAX_PAGE] = {0};
  int _dividerRawValue = 0;
  int _divider = 6; // 16th notes
  int _lastStepRawValue = 0;
  uint8_t _lastStep = 15;
  uint16_t _currentStep = 0; // hold the value for the current step 0-63
  bool _hasPulse = false;
  bool _output = false;
  unsigned long _lastOutputTime = 0;
  unsigned long _lastPulseTime = 0;
  uint8_t _outputPin;
  bool _outputLedState = false;
  bool _dividerLedState = false;
  uint8_t _dividerLedCounter = 0;
  AnalogInput* _dividerPot;
  LastStepSet _onLastStepSet = nullptr;
  TempoDividerSet _onTempoDividerSet = nullptr;



  void SetLastStep(int raw, uint8_t page) {
    if(raw < 0 || raw > 1023) return;
    _lastStepRawValue = raw;

    /* 
    * page = 0: last step =>  0-15
    * page = 1: last step =>  1-31
    * page = 2: last step => 32-47
    * page = 3: last step => 48-63
    */
    _lastStep = (page * STEPS_PR_PAGE) + MapToByte(raw, 0, 15);

    // Serial.print("last step: ");
    // Serial.println(_lastStep);
    
  }

  void SetTempoDivider(int raw) {
    if(raw < 0 || raw > 1023) return;
    /* what kind of dividers do we want?
    *
    * type          24 ppqn pulses    map (0-1023)      comments
    * 32th note     3                 0-99
    * 16th note     6                 100-299           this is the most used, so the map range should be bigger than other
    * tripled 16ths 8                 300-399
    * dotted 16th   9                 400-499
    * 8th note      12                500-599
    * triplet 8ths  15                600-699 
    * 4th notes     24                700-799
    * 
    */
    _dividerRawValue = raw;
    if(raw < 100) _divider = 3;
    else if(raw < 300) _divider = 6;
    else if(raw < 400) _divider = 8;
    else if(raw < 500) _divider = 9;
    else if(raw < 600) _divider = 12;
    else if(raw < 700) _divider = 15;
    else if(raw < 800) _divider = 24;
    else _divider = 6;
  }  


public:
  Channel(uint8_t channelNumber, uint8_t outputPin, uint8_t dividerPotPin) { 
    _channelNumber = channelNumber;
    _outputPin = outputPin;
    pinMode(_outputPin, OUTPUT);
    digitalWrite(_outputPin, LOW);
    _dividerPot = new AnalogInput(dividerPotPin);
  }

  void Begin() {
    _dividerPot->Begin();
  }

  
  void OnLastStepSet(LastStepSet handler) {
    _onLastStepSet = handler;
  }

  void OnTempoDividerSet(TempoDividerSet handler) {
    _onTempoDividerSet = handler;
  }

  bool Enabled() {
    return _enabled;
  }

  void SetEnabled(bool value) {
    _enabled = value;
  }

  void SetSteps(uint8_t page, uint16_t steps) {
    if(page < 0 || page > MAX_PAGE) return;
    _steps[page] = steps;
    //UpdateCurrentPattern();
  }

  uint16_t GetStepsByPage(uint8_t page, bool withIndicator) {
    if(page < 0 || page > MAX_PAGE) return;
    uint16_t steps = _steps[page];
    if(withIndicator) {
      uint8_t pageStartStep = page * STEPS_PR_PAGE;
      uint8_t pageEndStep = pageStartStep + STEPS_PR_PAGE;
      if(_currentStep >= pageStartStep && _currentStep < pageEndStep) {
        uint8_t stepIndex = _currentStep % STEPS_PR_PAGE;
        uint16_t stepIndicator = 1 << (15-stepIndex);
        steps |= stepIndicator;
      } 
    }
    return steps;
  }

  /*
  * advances the current step based on the pulses
  * returns 1 if the channel should output a pulse
  */
  bool Pulse(uint8_t pulses, unsigned long now) {
    // happens 24 times pr quarter node 
    _hasPulse = true;
    if(pulses % _divider == 0) {
      if(_currentStep < _lastStep)
        _currentStep++;
      else
        _currentStep=0;
      
      _dividerLedCounter++;
      if(_dividerLedCounter >= 4) { // only blink every on every 4th divier to make it observable - otherwise it would blink too fast to notice the differences
        _dividerLedCounter = 0;
        _dividerLedState = _enabled; // only set the divider led high if the channel is enabled
      }

      _lastPulseTime = now;

      uint8_t page = _currentStep / 16;
      uint16_t stepInPage = _currentStep % 16;
      uint16_t pageSteps = GetStepsByPage(page, false);

      if(_channelNumber==0) {
      char s[100];
      sprintf(s, "currentpage: %d  currentstep: %d  laststep: %d", page, _currentStep, _lastStep);
      Serial.println(s);     
      } 

      _output = _enabled && (pageSteps & (1 << (15 - stepInPage))) != 0;

      if(_output) {
        digitalWrite(_outputPin, HIGH);
        _lastOutputTime = now;
        _outputLedState = true;
      }

      return _output;
    } else {
      return false;
    }
  }

  uint8_t CurrentStep() {
    return _currentStep;
  }

  unsigned long LastOutputTime() {
    return _lastOutputTime;
  }

  bool OutputLedState() {
    return _outputLedState;
  }

  bool DividerLedState() {
    return _dividerLedState;
  }

  uint8_t LastStep() {
    return _lastStep;
  }

  void Run(unsigned long now, bool setLastStepMode, uint8_t page) {
    // update output, led and timing
    if (now > (_lastOutputTime + OUTPUT_PULSE)) {
      digitalWrite(_outputPin, LOW);
    }
    if (now > (_lastOutputTime + LED_SHORT_PULSE)) {
      _outputLedState = false;
    }    
    if (_dividerLedState && (now > (_lastPulseTime + LED_SHORT_PULSE))) {
      _dividerLedState = false;
    }
    
    // read divider pot
    uint16_t analogValue;
    if(_dividerPot->Changed(now, analogValue)) {
      if(setLastStepMode) { // set last step
        SetLastStep(analogValue, page);
        if(_onLastStepSet)
          _onLastStepSet(_channelNumber, _lastStep);
      } else { // set divider
        SetTempoDivider(analogValue);
        if(_onTempoDividerSet)
          _onTempoDividerSet(_channelNumber, _divider);
      }
    }
  }

  void StartPotCatchUp() {
    _dividerPot->StartCatchUp(); 
  }

  void StartSetLastStep() {
    _dividerPot->StartCatchUp(); 
    _dividerPot->SetReferenceValue(_lastStepRawValue);
  }

  void StartSetDivider() {
    _dividerPot->StartCatchUp(); 
    _dividerPot->SetReferenceValue(_dividerRawValue);
  }

  void Reset() {
    _hasPulse = false;
    _currentStep = 0;
    _lastPulseTime = 0; // now?
    _lastOutputTime = 0; // now?
    _dividerLedState = false;
    _dividerLedCounter = 0;
    _output = false;
    digitalWrite(_outputPin, LOW);
  }

  uint8_t ChannelNumber() {
    return _channelNumber;
  }

  void LoadPartData(DrumSequencer data) {
    _enabled = data.channel[_channelNumber].enabled;
    _divider = data.channel[_channelNumber].divider;
    _lastStep = data.channel[_channelNumber].lastStep;
    if(_onLastStepSet)
      _onLastStepSet(_channelNumber, _lastStep);    
    for(int i=0; i<4; i++) {
      _steps[i] = data.channel[_channelNumber].page[i];
    }
    _outputLedState = _enabled;
  }
};



#endif