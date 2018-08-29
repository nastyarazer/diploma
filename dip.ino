#include <Wire.h>
#include <SparkFun_VL53L1X_Arduino_Library.h>
#include <WiFi.h>
#include <esp_wps.h>
#include <EEPROM.h>
#include <ESP32WebServer.h>  //https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
#include <WiFiClient.h>

//Time of Flight sensor
VL53L1X distanceSensor;
//WPS
#define ESP_WPS_MODE      WPS_TYPE_PBC
#define ESP_MANUFACTURER  "ESPRESSIF"
#define ESP_MODEL_NUMBER  "ESP32"
#define ESP_MODEL_NAME    "ESPRESSIF IOT"
#define ESP_DEVICE_NAME   "ESP STATION"

static esp_wps_config_t config;
//End of WPS.

//Ultrasonic sensor
#define header_H    0x55 //Header
#define header_L    0xAA //Header
#define device_Addr 0x11 //Address
#define data_Length 0x00 //Data length
#define get_Dis_CMD 0x02 //Command: Read Distance
#define checksum    (header_H+header_L+device_Addr+data_Length+get_Dis_CMD) //Checksum

unsigned char i = 0;
//WEB SERVER

String webpage = ""; // General purpose variable to hold HTML code

//String field1, field4, field5 , CheckBoxChoice = "";
//float  field2 ;
//int    field3 ;
String field4, field5, CheckBoxChoice = "";
int field1,field2, field3;
  

ESP32WebServer server(80); // Start server on port 80 (default for a web-browser, change to your requirements, e.g. 8080 perhaps, if your Router uses port 80
// To access server from outside of a WiFi (LAN) network e.g. on port 8080 add a rule on your Router that forwards a connection request
// to http://your_network_WAN_address:8080 to http://your_network_LAN_address:8080 and then you can view your ESP server from anywhere.
// Example http://yourhome.ip:8080 and your ESP Server is at 192.168.0.40, then the request will be directed to http://192.168.0.40:8080
//
//unsigned int  ULTSON_distance=0;
unsigned char Rx_DATA[8];
unsigned char CMD[6] = {
  header_H, header_L, device_Addr, data_Length, get_Dis_CMD, checksum
}; //Distance command package
//End of Ultrasonic sensor

//Time to Flight sensor
uint8_t shortRange = 0;
uint8_t midRange = 1;
uint8_t longRange = 2;
//TOF end.

//Deep sleep settings
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;
//
//LED control
const int ledCount = 3;    // the number of LEDs in the bar graph
int ledPins[] = {
  13, 12, 27
};   // an array of pin numbers to which LEDs are attached
//

//structure saved to EEPROM
struct configStruct {
  //distance
  int maxDistance;//90% of tank volume
  int minDistance; //0% of tank volume
  //key
  int key;
};
//state:
//0 - no configuration, initial setup;
//1 - need config from outside;
//2 - normal work;
int state;

//read structure from memmory EEPROM
configStruct structure;

//address in EEPROM
int address = 0;
//Setup
void setup() {

  Serial2.begin(19200);  //Serial1: Ultrasonic Sensor Communication Serial Port, Buadrate: 19200
  Serial.begin(9600); //Serial: ToF Sensor Comunication Serial Port, Buardrate: 9600
  Serial.begin(115200);
  delay(100);
  Wire.begin();
  for (int i = 0; i < ledCount ; i++) {
    pinMode (ledPins[i], OUTPUT);
  }

      structure = {0,0,0};
      storeStruct(&structure, sizeof(structure));

  if (distanceSensor.begin() == false)
  {
    Serial.println("Sensor offline!");
  }

  //Call setDistanceMode with 0, 1, or 2 to change the sensing range.
  distanceSensor.setDistanceMode(longRange);

  loadStruct(&structure, sizeof(structure));

  if (isEEPROMempty(structure)) {
    Serial.println("EEPROM is empty state is 0");
    //wifi connection
    WPS_con();
    while (WiFi.status() != WL_CONNECTED) {
      delay(5);
      Serial.println("Connecting to WiFi..");
      for (int i = 0; i < ledCount ; i++) {
        blinkf(ledPins[i]);
      }
    }
    Serial.println("Connected to :" + String(WiFi.SSID()));
    server_setup();
    state = 0;
  }
  else {
    Serial.println("EEPROM has data state is 2");
    WIFI_con();
    state = 2;
  }
}
//Main loop
void loop() {
  switch (state) {
    case 0: {
        Serial.println("State is 0");
        state = 1;
      }
      break;
    case 1: {
        Serial.println("State is 1");
        
        //receive data while connected to the server
        //receive configuration from user and write to EEPROM
        //change state to 2 for normal mode
        while(isEEPROMempty(structure)){
          server.handleClient();
//          int first_measure = distanceCalculation(ToF_get(), UART_get());
//          structure = {first_measure, first_measure, 123};
//          storeStruct(&structure, sizeof(structure));
        }
        state = 2;
      }
      break;
    case 2: {
        //normal work
        Serial.println("State is 2");
        //normal mode
        //      loadStruct(&structure, sizeof(structure));
        Serial.println("Original structure is:");
        Serial.println(structure.maxDistance);
        Serial.println(structure.minDistance);
        Serial.println(structure.key);
        main_program();
        delay(1000);
        //deepSleep();
      }
      break;

  }
}

void main_program() {

  server.handleClient();

  configStruct temp_struct;

  loadStruct(&temp_struct, sizeof(temp_struct));

  int min_dist = structure.minDistance;
  int max_dist = structure.maxDistance;
  float level = 0.0;
  Serial.println("passed");
  int measured_dist;

  for (int j = 0; j < 2; j++) {
    measured_dist = distanceCalculation(ToF_get(), UART_get());
  }
  Serial.println("Measured: " + String(measured_dist));
  //delay(100);
  //measured_dist can be compared to configured distance
  if (measured_dist > max_dist) {
    structure.maxDistance = measured_dist;
    Serial.println("Max: " + String(structure.maxDistance));
  } else if (measured_dist < min_dist) {
    structure.minDistance = measured_dist;
    Serial.println("Min: " + String(structure.minDistance));
  }

  if (structure.minDistance != temp_struct.minDistance || structure.maxDistance != temp_struct.maxDistance) {

    storeStruct(&structure, sizeof(structure));
    loadStruct(&structure, sizeof(structure));
    Serial.println("Saved struct:");
    Serial.println(structure.maxDistance);
    Serial.println(structure.minDistance);
    Serial.println(structure.key);
  }
  //show led
  level = (max_dist - measured_dist);
  Serial.println(max_dist);
  Serial.println(measured_dist);
  Serial.println(level);
  float lev = level / max_dist;
  Serial.println(lev);
  if (lev >= 0 && lev <= 0.3) {
    digitalWrite(ledPins[1], LOW);
    digitalWrite(ledPins[2], LOW);
    digitalWrite(ledPins[0], HIGH);
    Serial.println("GREEN");
    delay(500);
  } else if (lev >= 0.4 && lev <= 0.6) {
    digitalWrite(ledPins[0], LOW);
    digitalWrite(ledPins[2], LOW);
    digitalWrite(ledPins[1], HIGH);
    Serial.println("YELLOW");
    delay(500);
  } else if (lev >= 0.6 && lev <= 0.9) {
    digitalWrite(ledPins[0], LOW);
    digitalWrite(ledPins[1], LOW);
    digitalWrite(ledPins[2], HIGH);
    Serial.println("RED");
    delay(500);
  }
  delay(1000);

  //connection to web service check connection
  //deep

}
void WIFI_con() {
  WiFi.begin();
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    esp_wifi_wps_start(0);
    Serial.println("Connecting to WiFi..");

    for (int i = 0; i < ledCount ; i++) {
      blinkf(ledPins[i]);
    }
  }
  Serial.println("Connected to :" + String(WiFi.SSID()));
  server_setup();
}


bool isEEPROMempty(configStruct initStruct) {
  //EPPROM.get(0,structure);
  //hash = hash functon of all fields in structure except hash itself
  //hash comparison of data from eprom and calculated one use hash function
  //true false output
  int pass_key = 123;
  if (initStruct.key == pass_key) {
    return false;
  } else {
    return true;
  }

}
//EEPROM write structure
void storeStruct(void *data_source, size_t size)
{
  EEPROM.begin(size * 2);
  for (size_t i = 0; i < size; i++)
  {
    char data = ((char *)data_source)[i];
    EEPROM.write(i, data);
  }
  EEPROM.commit();
}

//EEPROM load stucture
void loadStruct(void *data_dest, size_t size)
{
  EEPROM.begin(size * 2);
  for (size_t i = 0; i < size; i++)
  {
    char data = EEPROM.read(i);
    ((char *)data_dest)[i] = data;
  }
}
//Ultrasonic sensor
int UART_get() {

  int ULTSON_distance = 0;
  int ULTSON_final = 0;

  //take 10 measurements
  for (int k = 0; k < 10; k++) {
    for (i = 0; i < 6; i++) {
      Serial2.write(CMD[i]);
    }
    delay(10);  //Wait for the result

    i = 0; //reset counter

    while (Serial2.available()) { //Read the return data
      Rx_DATA[i++] = (Serial2.read());
    }
    ULTSON_distance = ((Rx_DATA[5] << 8) | Rx_DATA[6]); //Read the distance value
    ULTSON_final = ULTSON_final + ULTSON_distance;
  }
  ULTSON_final = ULTSON_final / 10;
  Serial.println("ULTSON: " + String(ULTSON_final));

  return ULTSON_final;//cm
}

//Time to flight sensor
int ToF_get() {

  int ToF_distance = 0;
  int ToF_final = 0;

  //take 10 measurements
  for (int k = 0; k < 10; k++) {
    while (distanceSensor.newDataReady() == false) {
      delay(10);
    }

    ToF_distance = distanceSensor.getDistance() / 10; //Get the result of the measurement from the sensor
    ToF_final = ToF_final + ToF_distance;
  }
  ToF_final = ToF_final / 10;

  Serial.println("ToF: " + String(ToF_final));

  return ToF_final;//cm
}

int distanceCalculation(int ToF_result, int ULTSON_result) {
  //mean value of results from two sensors
  int final_distance = (ToF_result + ULTSON_result) / 2;
  return final_distance;
}
/*
  Method to print the reason by which ESP32
  has been awaken from sleep
*/
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case 3  : Serial.println("Wakeup caused by timer"); break;
    case 4  : Serial.println("Wakeup caused by touchpad"); break;
    case 5  : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.println("Wakeup was not caused by deep sleep"); break;
  }
}
void deepSleep() {
  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  /*
    First we configure the wake up source
    We set our ESP32 to wake up every 5 seconds
  */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                 " Seconds");

  /*
    Next we decide what all peripherals to shut down/keep on
    By default, ESP32 will automatically power down the peripherals
    not needed by the wakeup source, but if you want to be a poweruser
    this is for you. Read in detail at the API docs
    http://esp-idf.readthedocs.io/en/latest/api-reference/system/deep_sleep.html
    Left the line commented as an example of how to configure peripherals.
    The line below turns off all RTC peripherals in deep sleep.
  */
  //esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  //Serial.println("Configured all RTC Peripherals to be powered down in sleep");

  /*
    Now that we have setup a wake cause and if needed setup the
    peripherals state in deep sleep, we can now start going to
    deep sleep.
    In the case that no wake up sources were provided but deep
    sleep was started, it will sleep forever unless hardware
    reset occurs.
  */
  Serial.println("Going to sleep now");
  delay(1000);
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}
//WPS Config
void wpsInitConfig() {
  config.crypto_funcs = &g_wifi_default_wps_crypto_funcs;
  config.wps_type = ESP_WPS_MODE;
  strcpy(config.factory_info.manufacturer, ESP_MANUFACTURER);
  strcpy(config.factory_info.model_number, ESP_MODEL_NUMBER);
  strcpy(config.factory_info.model_name, ESP_MODEL_NAME);
  strcpy(config.factory_info.device_name, ESP_DEVICE_NAME);
}

String wpspin2string(uint8_t a[]) {
  char wps_pin[9];
  for (int j = 0; j < 8; j++) {
    wps_pin[j] = a[j];
  }
  wps_pin[8] = '\0';
  return (String)wps_pin;
}

void WiFiEvent(WiFiEvent_t event, system_event_info_t info) {
  switch (event) {
    case SYSTEM_EVENT_STA_START:
      Serial.println("Station Mode Started");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("Connected to :" + String(WiFi.SSID()));
      Serial.print("Got IP: ");
      Serial.println(WiFi.localIP());
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("Disconnected from station, attempting reconnection");
      WiFi.reconnect();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      Serial.println("WPS Successfull, stopping WPS and connecting to: " + String(WiFi.SSID()));
      esp_wifi_wps_disable();
      delay(10);
      WiFi.begin();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
      Serial.println("WPS Failed, retrying");
      esp_wifi_wps_disable();
      esp_wifi_wps_enable(&config);
      esp_wifi_wps_start(0);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
      Serial.println("WPS Timedout, retrying");
      esp_wifi_wps_disable();
      esp_wifi_wps_enable(&config);
      esp_wifi_wps_start(0);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:
      Serial.println("WPS_PIN = " + wpspin2string(info.sta_er_pin.pin_code));
      break;
    default:
      break;
  }
}
void WPS_con() {

  delay(10);

  Serial.println();

  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_MODE_STA);

  Serial.println("Starting WPS...");

  wpsInitConfig();
  esp_wifi_wps_enable(&config);
  esp_wifi_wps_start(0);
}
//server
void server_setup() {
  server.on("/",      showInput);  // Comment out if not required
  server.on("/userinput", userinput);  // Must retain if user input is required
  server.onNotFound(handleNotFound);   // If the user types something that is not supported, say so
  server.begin(); Serial.println(F("Webserver started...")); // Start the webserver
}
//LED blinking
void blinkf(const byte which) {
  digitalWrite(which, HIGH);
  delay(500);
  digitalWrite(which, LOW);
  delay(500);
}
void userinput() {
  String field1_response, field2_response, field3_response, field4_response, field5_response;
  CheckBoxChoice = "";
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  String IPaddress = WiFi.localIP().toString();
  append_HTML_header();
  webpage += "<h3>User Input, enter values then select Enter</h3>";
  webpage += "<form action=\"http://" + IPaddress + "/userinput\" method=\"POST\">";
  webpage += "<table style='font-family:arial,sans-serif;font-size:16px;border-collapse:collapse;text-align:center;width:90%;margin-left:auto;margin-right:auto;'>";
  webpage += "<tr>";
  webpage += "<th style='border:0px solid black;text-align:left;padding:2px;'>Set MAX(when tank if full):</th>";
  webpage += "<th style='border:0px solid black;text-align:left;padding:2px;'>Set MIN(when tank is empty):</th>";
  webpage += "<th style='border:0px solid black;text-align:left;padding:2px;'>KEY</th>";
  webpage += "<th style='border:0px solid black;text-align:left;padding:2px;'>Input Field 4</th>";
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td style='border:0px solid black;text-align:left;padding:2px;'><input type='text' name='field1' value='0-350'></td>";
  webpage += "<td style='border:0px solid black;text-align:left;padding:2px;'><input type='text' name='field2' value='0-350'></td>";
  webpage += "<td style='border:0px solid black;text-align:left;padding:2px;'><input type='text' name='field3' value='123'></td>";
  webpage += "<td style='border:0px solid black;text-align:left;padding:2px;'><input type='text' name='field4' value='Text entry'></td>";
  webpage += "</tr>";
  webpage += "</table><br><br>";
  webpage += "Input field 5<br><input type='text' name='field5' value='field-5-default'><br><br>";
  // And so-on
  webpage += "<input type='checkbox' name='CheckBoxChoice' value='a'>Option-A";
  webpage += "<input type='checkbox' name='CheckBoxChoice' value='b'>Option-B";
  webpage += "<input type='checkbox' name='CheckBoxChoice' value='c'>Option-C";
  webpage += "<input type='checkbox' name='CheckBoxChoice' value='d'>Option-D";
  // And so-on
  webpage += "<br><br><input type='submit' value='Enter'><br><br>";
  webpage += "</form></body>";
  //  append_HTML_footer();
  server.send(200, "text/html", webpage); // Send a response to the client to enter their inputs, if needed, Enter=defaults
  if (server.args() > 0 ) { // Arguments were received
    for ( uint8_t i = 0; i < server.args(); i++ ) {
      String Argument_Name   = server.argName(i);
      String client_response = server.arg(i);
      if (Argument_Name == "field1") field1_response = client_response;
      if (Argument_Name == "field2") field2_response = client_response;
      if (Argument_Name == "field3") field3_response = client_response;
      if (Argument_Name == "field4") field4_response = client_response;
      if (Argument_Name == "field5") field5_response = client_response;
      if (Argument_Name == "CheckBoxChoice") {
        if (client_response.length() > 1)
          CheckBoxChoice = "a"; else CheckBoxChoice = client_response; // Checking for more than one check-box being selected too, 'a' if more than one
      }
    }
  }
//  field1 = field1_response;
//  field2_response.trim(); // Remove any leading spaces
//  field3_response.trim(); // Remove any leading spaces
//  if (isValidNumber(field2_response)) field2 = field2_response.toFloat();
//  if (isValidNumber(field3_response)) field3 = field3_response.toInt();
//  field4 = field4_response;
//  field5 = field5_response;
  if (isValidNumber(field1_response)) structure.maxDistance = field1_response.toInt();
  if (isValidNumber(field2_response)) structure.minDistance = field2_response.toInt();
  if (isValidNumber(field3_response)) structure.key= field3_response.toInt();
  storeStruct(&structure, sizeof(structure));
//  Serial.println("   Field1 Input was : " + field1);
//  Serial.println("   Field2 Input was : " + String(field2, 6));
//  Serial.println("   Field3 Input was : " + String(field3));
  Serial.println("   MAX Input was : " + structure.maxDistance);
  Serial.println("   MIN Input was : " + structure.minDistance);
  Serial.println("   KEY Input was : " + structure.key);
  Serial.println("   Field4 Input was : " + field4);
  Serial.println("   Field5 Input was : " + field5);
  Serial.println("Checkbox choice was : " + CheckBoxChoice);
  server.send(200, "text/html", webpage);
}

boolean isValidNumber(String str) {
  str.trim();
  if (!(str.charAt(0) == '+' || str.charAt(0) == '-' || isDigit(str.charAt(0)))) return false; // Failed if not starting with +- or a number
  for (byte i = 1; i < str.length(); i++) {
    if (!(isDigit(str.charAt(i)) || str.charAt(i) == '.')) return false; // Anything other than a number or . is a failure
  }
  return true;
}

void showInput() {
  append_HTML_header();
  webpage += "<H3>This was the User Input</H3>";
  webpage += "<p class='style2'>";
  webpage += "Field-1 user input was: " + String(field1) + "<br>";
  webpage += "Field-2 user input was: " + String(field2) + "<br>";
  webpage += "Field-3 user input was: " + String(field3) + "<br>";
  webpage += "Field-4 user input was: " + field4 + "<br>";
  webpage += "Field-5 user input was: " + field5 + "<br>";
  webpage += "   Checkbox choice was: " + CheckBoxChoice + "<br>";
  webpage += "</p>";
  server.send(200, "text/html", webpage);
}
void handleNotFound() {
  String message = "The request entered could not be found, please try again with a different option\n";
  server.send(404, "text/plain", message);
}
void append_HTML_header() {
  webpage  = "";
  webpage += "<!DOCTYPE html><html><head>";
  webpage += "<meta http-equiv='refresh' content='600'>"; // 5-min refresh time, test needed to prevent auto updates repeating some commands
  webpage += "<style>";
  webpage += "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  webpage += "body {width:device-width;margin:0 auto;font-family:arial;font-size:14px;text-align:center;}";
  webpage += "text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  webpage += "h1 {margin:16px 30px;}"; // Orange background
  webpage += "h3 {font-size:24px;width:auto;}";
  webpage += ".style1{text-align:center;font-size:16px;background-color:#FFE4B5;}";
  webpage += ".style2{text-align:left;font-size:16px;background-color:#F7F2Fd;width:auto;margin:0 auto;}";
  webpage += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;";
  webpage += "text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  webpage += "</style>";
  webpage += "</head><body>";
  webpage += "<div class='navbar'>";
  webpage += " <a href='/userinput'>Get Input</a>";
  webpage += " <a href='/user'>Show Input</a>";
}

