#include "esphome.h"
//#include "String.h"

class MyCustomSwitch : public Component, public Switch {
 public:  
  char button;
  //MyCustomSwitch(char _button) : Switch { button = _button; }
  void setup() override {
    ESP_LOGD("ROWDY", "SETUP");	
	//button = 1;
  }
  
  void setButton(char _button) {
	  button = _button;
  }
  
  void handleUp() {
	ESP_LOGD("ROWDY", "Going UP!");  
  }
  
  void handleDown() {
	ESP_LOGD("ROWDY", "Going DOWN!");  
  }
  void handleProgram() {
	ESP_LOGD("ROWDY", "PROGRAMMING!");  
  }
  
  
  
  void write_state(bool state) override {
    // This will be called every time the user requests a state change.

    digitalWrite(5, state);
	switch(button)
	{
		case 'u':
			ESP_LOGD("ROWDY", "Going UP!");
			break;
		case 'd':
			ESP_LOGD("ROWDY", "Going DOWN");
			break;
	}

    // Acknowledge new state by publishing it
    publish_state(state);
  }
};