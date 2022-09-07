#include <esphome.h>
#include <EEPROM.h>


#define SYMBOL 640
#define UP 0x2
#define STOP 0x1
#define DOWN 0x4
#define PROG 0x8
#define EEPROM_ADDRESS 0
#define REMOTE 0x111111    //<-- Change it!


class CoverSomfiAPI : public Component, public CustomAPIDevice {
public:
	const int transmitPin = 5;
	unsigned long rollingCode = 1;
	byte frame[7];
	byte checksum;

	void setup() override {
		register_service(&CoverSomfiAPI::programSomfy, "program");
	}
	
	void programSomfy()
	{ 
		auto myCover = new CoverSomfiAPI();
		myCover->sendProgram();
	}
	


	void SendCommand(byte *frame, byte sync) {		
		if(sync == 2) { // Only with the first frame.
			//Wake-up pulse & Silence
			digitalWrite(transmitPin, HIGH);
			delayMicroseconds(9415);
			digitalWrite(transmitPin, LOW);
			delayMicroseconds(89565);
		}

		// Hardware sync: two sync for the first frame, seven for the following ones.
		for (int i = 0; i < sync; i++) {
			digitalWrite(transmitPin, HIGH);
			delayMicroseconds(4*SYMBOL);
			digitalWrite(transmitPin, LOW);
			delayMicroseconds(4*SYMBOL);
		}

		// Software sync
		digitalWrite(transmitPin, HIGH);
		delayMicroseconds(4550);
		digitalWrite(transmitPin, LOW);
		delayMicroseconds(SYMBOL);
	  
	  
		//Data: bits are sent one by one, starting with the MSB.
		for(byte i = 0; i < 56; i++) {
			if(((frame[i/8] >> (7 - (i%8))) & 1) == 1) {
				digitalWrite(transmitPin, LOW);
				delayMicroseconds(SYMBOL);
				digitalWrite(transmitPin, HIGH);
				delayMicroseconds(SYMBOL);
			}
			else {
				digitalWrite(transmitPin, HIGH);
				delayMicroseconds(SYMBOL);
				digitalWrite(transmitPin, LOW);
				delayMicroseconds(SYMBOL);
			}
		}
	  
		digitalWrite(transmitPin, LOW);
		delayMicroseconds(30415); // Inter-frame silence
	}
	
	void BuildFrame(byte *frame, byte button) {
		unsigned int code;
	    EEPROM.get(EEPROM_ADDRESS, code);
	    frame[0] = 0xA7; 		 // Encryption key. Doesn't matter much
		frame[1] = button << 4;  // Which button did  you press? The 4 LSB will be the checksum
		frame[2] = code >> 8;    // Rolling code (big endian)
		frame[3] = code;         // Rolling code
		frame[4] = REMOTE >> 16; // Remote address
		frame[5] = REMOTE >>  8; // Remote address
		frame[6] = REMOTE;       // Remote address
		ESP_LOGD("SOMFY", "Frame: %d", frame);
 
		// Checksum calculation: a XOR of all the nibbles
		checksum = 0;
		for(byte i = 0; i < 7; i++) {
			checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
		}
		checksum &= 0b1111; // We keep the last 4 bits only
		frame[1] |= checksum; // If a XOR of all the nibbles is equal to 0, the blinds will consider the checksum ok.
		ESP_LOGD("SOMFY", "With checksum: %d", frame); 
	  
		// Obfuscation: a XOR of all the bytes
		for(byte i = 1; i < 7; i++) {
			frame[i] ^= frame[i-1];
		}
		ESP_LOGD("SOMFY", "Obfuscated: %d", frame);		

		//  We store the value of the rolling code in the EEPROM. It should take up to 2 adresses but the Arduino function takes care of it.
		ESP_LOGD("SOMFY", "Rolling Code: %d", code);
		EEPROM.put(EEPROM_ADDRESS, code + 1);  
		EEPROM.commit();                                       
	}
	
	void send(byte button)
	{
		pinMode(transmitPin, OUTPUT);
		digitalWrite(transmitPin, LOW);
		
		EEPROM.begin(4);
		EEPROM.get(EEPROM_ADDRESS, rollingCode);
		
		ESP_LOGD("SOMFY", "Simulated remote number: %d", REMOTE);
		ESP_LOGD("SOMFY", "Current rolling code: %d", rollingCode);
		
		BuildFrame(frame, button);
		
		ESP_LOGD("SOMFY", "Sending...");
		SendCommand(frame, 2);
		for(int i = 0; i<2; i++) {
		  SendCommand(frame, 7);
		}
		ESP_LOGD("SOMFY", "Sent...");
	}
	
	void sendStop()
	{
		ESP_LOGI("SOMFY", "STOP");		
		send(STOP);
	}
	void sendUp()
	{
		ESP_LOGI("SOMFY", "UP");		
		send(UP);
	}
	void sendDown()
	{
		ESP_LOGI("SOMFY", "DOWN");		
		send(DOWN);
	}
	
	void sendProgram() {
		ESP_LOGI("SOMFY", "PROGRAMMING");
		send(PROG);
	}
	
};

class SomfyCover : public Component, public Cover {
	public:
  
	CoverTraits get_traits() override {
		auto traits = CoverTraits();
		traits.set_is_assumed_state(false);
		traits.set_supports_position(true);
		traits.set_supports_tilt(false);
		return traits;
	}
	
	void control(const CoverCall &call) override {
		auto coverApi = new CoverSomfiAPI();
		
		if (call.get_stop()) {
			coverApi->sendStop();
		}
		if (call.get_position().has_value()) {
			uint8_t pos = *call.get_position() * 100.0f;
			if (pos == 100) {
				coverApi->sendUp();
			} else if (pos == 0) {
				coverApi->sendUp();
			} else {
				ESP_LOGW("SOMFY", "UNKNOWN, pos = %d", pos);
			}

			this->position = pos;
			this->publish_state();
		}
	}
};

