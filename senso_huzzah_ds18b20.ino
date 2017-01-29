
/*
  ESP8266 sensor post server

  This is an example of announcing and finding services.
  
  Instructions:
  - Update WiFi SSID and password as necessary.
  - Flash the sketch to ESP8266 boards
  - Used in conjuction with the sensorelay sofware at https://github.com/charles-d-burton/sensorelay will post data to the relay
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <ESP8266TrueRandom.h>
#include "FS.h"

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 4
#define TEMPERATURE_PRECISION 9 // Lower resolution

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

int numberOfDevices; // Number of temperature devices found

DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address

const char* ssid     = "Ilikechicken";
const char* password = "******";
char hostString[16] = {0};
byte uuidNumber[16];

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\r\nsetup()");
  getUUID();

  sprintf(hostString, "ESP_%06X", ESP.getChipId());
  Serial.print("Hostname: ");
  Serial.println(hostString);
  WiFi.hostname(hostString);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  discoverRelay();
  setupSensors();
}

void discoverRelay() {
  if (!MDNS.begin(hostString)) {
    Serial.println("Error setting up MDNS responder!");
  }
  
  Serial.println("Sending mDNS query");
  int n = MDNS.queryService("sensorelay", "tcp"); // Send out query for esp tcp services
  Serial.println("mDNS query done");
  if (n == 0) {
    Serial.println("no services found");
  }
  else {
    Serial.print(n);
    Serial.println(" service(s) found");
    for (int i = 0; i < n; ++i) {
      // Print details for each service found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(MDNS.hostname(i));
      Serial.print(" (");
      Serial.print(MDNS.IP(i));
      Serial.print(":");
      Serial.print(MDNS.port(i));
      Serial.println(")");
    }
  }
  Serial.println();
}

void setupSensors() {
  // Start up the library
  sensors.begin();
  
  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  
  // locate devices on the bus
  Serial.print("Locating devices...");
  
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
  
  // Loop through each device, print out address
  for(int i=0;i<numberOfDevices; i++)
  {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i))
  {
    Serial.print("Found device ");
    Serial.print(i, DEC);
    Serial.print(" with address: ");
    printAddress(tempDeviceAddress);
    Serial.println();
    
    Serial.print("Setting resolution to ");
    Serial.println(TEMPERATURE_PRECISION, DEC);
    
    // set the resolution to TEMPERATURE_PRECISION bit (Each Dallas/Maxim device is capable of several different resolutions)
    sensors.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);
    
     Serial.print("Resolution actually set to: ");
    Serial.print(sensors.getResolution(tempDeviceAddress), DEC); 
    Serial.println();
  }else{
    Serial.print("Found ghost device at ");
    Serial.print(i, DEC);
    Serial.print(" but could not detect address. Check power and cabling");
  }
  }
}

void loop() {
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  
  
  // Loop through each device, print out temperature data
  for(int i=0;i<numberOfDevices; i++)
  {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i))
  {
    // Output the device ID
    Serial.print("Temperature for device: ");
    Serial.println(i,DEC);
    
    // It responds almost immediately. Let's print out the data
    printTemperature(tempDeviceAddress); // Use a simple function to print out the data
  } 
  //else ghost device! Check your power requirements and cabling
  
  }
  //delay(1000);

}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& sensor = root.createNestedObject("sensor");
  sensor["device"] = hostString;
  sensor["name"] = "testdevice";
  JsonObject& data = root.createNestedObject("data");
  data["tempC"] = tempC;
  data["tempF"] = DallasTemperature::toFahrenheit(tempC);
  char buffer[256];
  root.printTo(buffer, sizeof(buffer));
  updateRelay(buffer);
  root.printTo(Serial);
  Serial.println();
  
}

void updateRelay(char* message) {
  int n = MDNS.queryService("sensorelay", "tcp"); // Send out query for esp tcp services
  if (n == 0) {
    Serial.println("no services found");
  }
  else {
    Serial.print(n);
    Serial.println(" service(s) found");
    for (int i = 0; i < n; ++i) {
      String url = "http://";
      url.concat(MDNS.IP(i).toString());
      url.concat(":");
      String port = String(MDNS.port(i));
      url.concat(port);
      url.concat("/reading");
      HTTPClient http;
      http.begin(url);
      http.addHeader("Content-Type", "text/json");
      int httpCode = http.POST(message);
      String response = http.getString();
      Serial.print("Response code: ");
      Serial.println(httpCode);
      Serial.print("Response body: ");
      Serial.println(response);
      http.end();
    }
  }
}

void getUUID() {
  // Generate a new UUID
  // open the file in write mode
  Serial.println("Opening filsystem");
  //SPIFFS.format();
  bool fsopen = SPIFFS.begin();
  
  if (fsopen) {
    bool exists = SPIFFS.exists("/uuid.txt");
    File f = SPIFFS.open("/uuid.txt", "w");
    if (!exists) {
      ESP8266TrueRandom.uuid(uuidNumber);
      String uuidStr = ESP8266TrueRandom.uuidToString(uuidNumber);
      Serial.println("Setting UUID to: " + uuidStr);
      f.println(uuidStr);
      f.flush();
    } else {
      Serial.println("File Found!");
      //while(f.available()) {
        String line = f.readString();
        Serial.println("Found UUID!");
        Serial.println(line);
        line.getBytes(uuidNumber,16);
      //}
    }
    f.close();
  } else {
    Serial.println("Unable to open filesystem");
  }
  

}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
