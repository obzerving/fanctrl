/*
 * fanctrl - Control a fan remote
 * http://ESP_0AC8A4/pb?params=<devstate>
 * http://ESP_0AC8A4/zwh?params=on,${intensity.percent}
 */
#include <ESP8266WiFi.h>
#include <aREST.h>
#include <EEPROM.h>
#define clickspeed 100
struct ctrl_struct {
  char ctrl_name[10];
  byte ctrl_pin;
};
ctrl_struct fan[5];
aREST rest = aREST();
const char* ssid = "your_ssid";
const char* password = "your_password";
#define LISTEN_PORT           80
WiFiServer server(LISTEN_PORT);

// Variables to be exposed to the API
int devstate; // 0=off, 1=low, 2=med, 3=high
int level; // A percentage from Amazon Echo command

// Declare functions to be exposed to the API
int pushbutton(String command);
int zwh(String command);

void setup(void)
{
  // Start Serial
  Serial.begin(115200);
  strcpy(fan[0].ctrl_name, "FAN-OFF");
  fan[0].ctrl_pin = 5;
  strcpy(fan[1].ctrl_name, "FAN-LOW");
  fan[1].ctrl_pin = 12;
  strcpy(fan[2].ctrl_name, "FAN-MED");
  fan[2].ctrl_pin = 13;
  strcpy(fan[3].ctrl_name, "FAN-HIGH");
  fan[3].ctrl_pin = 14;
  strcpy(fan[4].ctrl_name, "FAN-LIGHT");
  fan[4].ctrl_pin = 2;

  for(int i=0; i<5; i++) {
    pinMode(fan[i].ctrl_pin, OUTPUT);
    digitalWrite(fan[i].ctrl_pin, LOW);
  }
  EEPROM.begin(512);
  byte value = EEPROM.read(0); // Check magic word
  if(value != B01100110)
  { // Not there, so set up EEPROM
    EEPROM.write(0, B01100110); // Magic word
    EEPROM.write(1, 0); // Current value of devstate
    EEPROM.write(2, 1); // Last "on" setting of devstate (init to Low)
    EEPROM.commit();
    devstate = 0;
  }
  else {
    devstate = EEPROM.read(1); // restore device state
  }
  level = devstate * 25;
  pushbutton(String(devstate));
  rest.variable("state", &devstate);
  rest.variable("level", &level);
  // Functions to be exposed
  rest.function("pb",pushbutton);
  rest.function("zwh", zwh);
  // Give name & ID to the device (ID should be 6 characters long)
  rest.set_id("1");
  rest.set_name("fanctrl");
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  // Start the server
  server.begin();
  Serial.println("Server started");
  // Print the IP address
  Serial.println(WiFi.localIP());
}

void loop() {
  // Handle REST calls
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  while(!client.available()){
    delay(1);
  }
  rest.handle(client);
}

// Custom function accessible by the API
int pushbutton(String command) {

  // Get button from command
  devstate = command.toInt();

  digitalWrite(fan[devstate].ctrl_pin,HIGH);
  delay(clickspeed);
  digitalWrite(fan[devstate].ctrl_pin,LOW);
  delay(clickspeed);
  EEPROM.write(1, devstate);
  EEPROM.commit();
  return devstate;
}

int zwh(String command) {
  // Get state and level from command (e.g. on,100)
  int i1 = command.indexOf(',');
  int pct;
  int ds;
  if(i1 == -1) { // no comma.
    if(command.substring(0) == "off") { // off command doesn't have a comma
      ds = 0;
      pct = 0;
    }
    if(command.substring(0) == "on") { // No level specified
       ds = EEPROM.read(2); // Use last devstate "on" setting
       pct = ds * 25; // and convert to a level within the range of devstate
    }
  } // i1 == -1
  else { // We have comma separated params (assume we have an on command)
    i1++; // point to beginning of level
    String lvl = command.substring(i1);
    if(lvl.length() > 0) {
      pct = lvl.toInt();
    }
    else { // No level param sent
     ds = EEPROM.read(2); // Use last devstate "on" setting
     pct = ds * 25; // and convert to a level within the range of devstate
    }
  }
  level = pct; // update level and devstate
  if(devstate > 0) EEPROM.write(2, devstate);
  if(pct == 0) devstate = 0;
  if((pct > 0) && (pct < 31)) devstate = 1;
  if((pct > 30) && (pct < 61)) devstate = 2;
  if(pct >= 60) devstate = 3;
  // Push the appropriate button
  digitalWrite(fan[devstate].ctrl_pin,HIGH);
  delay(clickspeed);
  digitalWrite(fan[devstate].ctrl_pin,LOW);
  delay(clickspeed);
  EEPROM.write(1, devstate);
  EEPROM.commit();
  return devstate;
}

