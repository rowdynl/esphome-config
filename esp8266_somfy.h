
/*   This sketch allows you to emulate a Somfy RTS or Simu HZ remote.
   If you want to learn more about the Somfy RTS protocol, check out https://pushstack.wordpress.com/somfy-rts-protocol/
   
   The rolling code will be stored in EEPROM, so that you can power the Arduino off.
   
   Easiest way to make it work for you:
    - Choose a remote number
    - Choose a starting point for the rolling code. Any unsigned int works, 1 is a good start
    - Upload the sketch
    - Long-press the program button of YOUR ACTUAL REMOTE until your blind goes up and down slightly
    - send 'p' to the serial terminal
  To make a group command, just repeat the last two steps with another blind (one by one)
  
  Then:
    - m, u or h will make it to go up
    - s make it stop
    - b, or d will make it to go down
    - you can also send a HEX number directly for any weird command you (0x9 for the sun and wind detector for instance)
*/

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

String header;
const char* ssid = "YOUR-WIRELESS-NETWORK";
const char* password = "YOUR-PASSWORD";


 
// Create an instance of the server
// specify the port to listen on as an argument
//WiFiServer server(80);
ESP8266WebServer server(80);

//#include <PubSubClient.h>



//#define PORT_TX 5 //5 of PORTD = DigitalPin 5

#define SYMBOL 640
#define UP 0x2
#define STOP 0x1
#define DOWN 0x4
#define PROG 0x8
#define EEPROM_ADDRESS 0
#define REMOTE 0x121309    //<-- Change it!

const int transmitPin = 5;

unsigned long rollingCode = 1;

byte frame[7];
byte checksum;


void BuildFrame(byte *frame, byte button) {
  unsigned int code;
  EEPROM.get(EEPROM_ADDRESS, code);
  frame[0] = 0xA7; // Encryption key. Doesn't matter much
  frame[1] = button << 4;  // Which button did  you press? The 4 LSB will be the checksum
  frame[2] = code >> 8;    // Rolling code (big endian)
  frame[3] = code;         // Rolling code
  frame[4] = REMOTE >> 16; // Remote address
  frame[5] = REMOTE >>  8; // Remote address
  frame[6] = REMOTE;       // Remote address

  Serial.print("Frame         : ");
  for(byte i = 0; i < 7; i++) {
    if(frame[i] >> 4 == 0) { //  Displays leading zero in case the most significant
      Serial.print("0");     // nibble is a 0.
    }
    Serial.print(frame[i],HEX); Serial.print(" ");
  }
  
// Checksum calculation: a XOR of all the nibbles
  checksum = 0;
  for(byte i = 0; i < 7; i++) {
    checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
  }
  checksum &= 0b1111; // We keep the last 4 bits only


//Checksum integration
  frame[1] |= checksum; //  If a XOR of all the nibbles is equal to 0, the blinds will
                        // consider the checksum ok.

  Serial.println(""); Serial.print("With checksum : ");
  for(byte i = 0; i < 7; i++) {
    if(frame[i] >> 4 == 0) {
      Serial.print("0");
    }
    Serial.print(frame[i],HEX); Serial.print(" ");
  }

  
// Obfuscation: a XOR of all the bytes
  for(byte i = 1; i < 7; i++) {
    frame[i] ^= frame[i-1];
  }

  Serial.println(""); Serial.print("Obfuscated    : ");
  for(byte i = 0; i < 7; i++) {
    if(frame[i] >> 4 == 0) {
      Serial.print("0");
    }
    Serial.print(frame[i],HEX); Serial.print(" ");
  }
  Serial.println("");
  Serial.print("Rolling Code  : "); Serial.println(code);
  EEPROM.put(EEPROM_ADDRESS, code + 1); //  We store the value of the rolling code in the
                                        // EEPROM. It should take up to 2 adresses but the
                                        // Arduino function takes care of it.
 EEPROM.commit();                                       
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

void setup() {
  Serial.begin(115200);
  Serial.println(" ");
  Serial.println("Starting Somfy");


 Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start the server
  server.on("/U", handleUp);
  server.on("/D", handleDown);
  server.on("/P", handleProg);
  server.on("/S", handleStop);
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());


  pinMode(transmitPin, OUTPUT);
  digitalWrite(transmitPin, LOW);
//  DDRD |= 1<<PORT_TX; // Pin 5 an output
//  PORTD &= !(1<<PORT_TX); // Pin 5 LOW

  EEPROM.begin(4);
  EEPROM.get(EEPROM_ADDRESS, rollingCode);

//  if (EEPROM.get(EEPROM_ADDRESS, rollingCode) < newRollingCode) {
//    EEPROM.put(EEPROM_ADDRESS, newRollingCode);
//  }
  
  Serial.println(" ");
  Serial.print("Simulated remote number : "); 
  Serial.println(REMOTE, HEX);
  Serial.print("Current rolling code    : "); 
  Serial.println(rollingCode);
}

void loop() {



 
  if (Serial.available() > 0) {
    char serie = (char)Serial.read();
    Serial.println("");
//    Serial.print("Remote : "); Serial.println(REMOTE, HEX);
    if(serie == 'm'||serie == 'u'||serie == 'h') {
      Serial.println("Monte"); // Somfy is a French company, after all.
      BuildFrame(frame, UP);
    }
    else if(serie == 's') {
      Serial.println("Stop");
      BuildFrame(frame, STOP);
    }
    else if(serie == 'b'||serie == 'd') {
      Serial.println("Descend");
      BuildFrame(frame, DOWN);
    }
    else if(serie == 'p') {
      Serial.println("Prog");
      BuildFrame(frame, PROG);
    }
    else {
      Serial.println("Custom code");
      BuildFrame(frame, serie);
    }

    Serial.println("");
    SendCommand(frame, 2);
    for(int i = 0; i<2; i++) {
      SendCommand(frame, 7);
    }
  }
  server.handleClient();  
    }
  
void handleUp() {            //Handler for the rooth path
 
  BuildFrame(frame, UP);
    SendCommand(frame, 2);
    for(int i = 0; i<2; i++) {
      SendCommand(frame, 7);
    }
  server.send(200, "text/plain", "up");
 
}
void handleDown() {            //Handler for the rooth path
 
  BuildFrame(frame, DOWN);
    SendCommand(frame, 2);
    for(int i = 0; i<2; i++) {
      SendCommand(frame, 7);
    }
    delay(15000);
BuildFrame(frame, STOP);
    SendCommand(frame, 2);
    for(int i = 0; i<2; i++) {
      SendCommand(frame, 7);
    }
  server.send(200, "text/plain", "down");
 
}
void handleProg() {            //Handler for the rooth path
 
  BuildFrame(frame, PROG);
    SendCommand(frame, 2);
    for(int i = 0; i<2; i++) {
      SendCommand(frame, 7);
    }
  server.send(200, "text/plain", "prog");
 
}
void handleStop() {            //Handler for the rooth path
 
  BuildFrame(frame, STOP);
    SendCommand(frame, 2);
    for(int i = 0; i<2; i++) {
      SendCommand(frame, 7);
    }
  server.send(200, "text/plain", "stop");
 
}

