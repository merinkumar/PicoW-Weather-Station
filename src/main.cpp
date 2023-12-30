#include <Arduino.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <string.h>
#include <WebServer.h>
#include <WiFi.h>
#include <hardware/sync.h>
#include <hardware/flash.h>
#include "DHT.h"
#include "hardware/watchdog.h"
#include "ThingSpeak.h"

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE) // Set the target offest to the last sector of flash
#define DHTPIN 6     // Digital pin connected to the DHT sensor
#define BMP280_ADDRESS 0x76  //I2C address of BMP280
#define OLED_ADDRESS 0x3C    //OLED Address
#define CHANNEL_ID 2287777 //your thingspeak channel number
#define CHANNEL_API_KEY "zzzzzzzzzz" //Your thingspeak api key
#define DELAY_LOOP 60000 // 1min delay
#define DHTTYPE DHT22

struct pageData
{
  int emptyFlag;
  char ssid[30];
  char pwd[30];
};

int *p, addr;
struct pageData writecred;
struct pageData readcred;
struct pageData *write_cred_pt;
struct pageData *read_cred_pt;
unsigned int page; // prevent comparison of unsigned and signed int
int first_empty_page = -1;
const int modePin = 0;
int buttonState = 0;
int modeCtr = 0;
int mode = 1; //0-> config update mode, 1-> regular mode


void init_stnmode(); 
void init_apmode();
void loop_stnmode();
void loop_apmode();
void init_storage();
void update_storage();
void get_storage();
void handle_onconnect();
void handle_update();
void handle_restart();
void handle_notfound();
void software_reset();
void setup_wifi();
String HTML();

String status = "Connected";
const char* ap_ssid = "PicoW-AP"; //Access Point SSID
const char* ap_password= "PicoW-AP-PWD"; //Access Point Password
uint8_t max_connections=1;//Maximum Connection Limit for AP
int current_stations=0, new_stations=0;
const char* mqtt_server = "mqtt.thingspeak.com";

IPAddress local_IP(192, 168, 4, 1);  // Set your desired static IP address
IPAddress gateway(192, 168, 4, 1);   // Usually the same as the IP address
IPAddress subnet(255, 255, 255, 0);
IPAddress IP;
WebServer server(80); //Specifying the Webserver instance to connect with HTTP Port: 80
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP280 bmp; // I2C
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);
WiFiClient picoClient;

void setup() {
  
  Serial.begin(115200); //Start the serial communication channel
  while (!Serial); // Wait untill serial is available
  Serial.println();

  pinMode(modePin, INPUT_PULLDOWN);
  
  while (modeCtr < 4) {
    delay(1000);
    buttonState = digitalRead(modePin);
    if (buttonState == 1){
        mode = 0;
    }
    modeCtr++;

  }

  Serial.printf("Mode : %d: ",mode);

  if (mode) {
    init_stnmode();
  } else {
    init_apmode();

  }

}
 
void loop() {
  
  if (mode) {
    loop_stnmode();
  } else {
    loop_apmode();

  }

}
 
void handle_onconnect()
{
  status = "Connected to AP";
  Serial.println("Client Connected");
  server.send(200, "text/html", HTML()); 
}
 
void handle_update()
{
  status = "Update initiated...";
  Serial.println("Update initiated...");
  String ssid_text = server.arg("ssid_txt");
  String pwd_text = server.arg("pwd_txt");

  strcpy(writecred.ssid, ssid_text.c_str());
  strcpy(writecred.pwd, pwd_text.c_str());

  update_storage();
  delay(2000);
  get_storage();

  status = "Update initiated...";
  server.send(200, "text/html", HTML());
  Serial.printf("SSID: %s\n", ssid_text.c_str());
  Serial.printf("Password: %s\n", pwd_text.c_str());

  status = "Update complete!";
  delay(1000);
  server.send(200, "text/html", HTML());
}
 
void handle_restart()
{
  status = "Restart initiated...";
  Serial.println("Restart initiated...");
  server.send(200, "text/html", HTML());
  delay(700);
  software_reset();
}
 

void handle_notfound()
{
  server.send(404, "text/plain", "Not found");
}
 
String HTML()
{
  String msg="<!DOCTYPE html> <html>\n";
  msg+="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  msg+="<title>Config Update</title>\n";
  msg+="<style>html {font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  msg+="body {margin-top: 50px;} h1 {color: #444444; margin: 50px auto 30px;} h3 {color: #444444; margin-bottom: 50px;}\n";
  msg+=".button {display: block; width: 180px; background-color: #0d81ec; border: none; color: white; padding: 13px 30px; text-decoration: none; font-size: 25px; margin: 0px auto 35px; cursor: pointer; border-radius: 4px;}\n";
  msg+=".button-update {background-color: #0d81ec;}\n";
  msg+=".button-update:active {background-color: #0d81ec;}\n";
  msg+=".button-restart {background-color: #ff0000;}\n";
  msg+=".button-restart:active {background-color: #ff0000;}\n";
  msg+=".textbox {width: 200px; height: 30px; border: none; background-color: #f48100; color: white; padding: 5px; font-size: 16px; border-radius: 4px; margin: 0px auto 35px;}\n";
  msg+=".status-field {width: 300px; height: 30px; border: none; background-color: #ffffff; color: black; padding: 5px; font-size: 16px; border-radius: 4px; margin: 0px auto 15px;}\n";
  msg+="</style>\n";
  msg+="</head>\n";
  msg+="<body>\n";
  msg+="<h1>PicoW Config Update</h1>\n";
  msg+="<h3>Using Access Point (AP) Mode</h3>\n";
  msg+="<form method='POST' action='/update'>\n";
  msg+="<input type=\"text\" class=\"textbox\" name=\"ssid_txt\" id=\"ssid_txt\" placeholder='Enter the NEW ssid'>\n";
  msg+="<input type=\"text\" class=\"textbox\" name=\"pwd_txt\" id=\"pwd_txt\" placeholder='Enter the NEW pwd'>\n";
  msg+="<input type='submit' value='Update' class=\"button button-update\">\n";
  msg+="</form>\n";
  msg+="<form method='POST' action='/restart'>\n";
  msg+="<input type='submit' value='Restart' class=\"button button-restart\">\n";
  msg+="</form>\n";
  msg+="<p>Status:</p>\n";

  msg+="<input type=\"text\" class=\"status-field\" readonly name=\"status_txt\" value=\"" + status + "\">\n";
   
  msg+="</body>\n";
  msg+="</html>\n";
  //Serial.printf("%s",msg.c_str());
  return msg;
}

void software_reset()
{
    watchdog_enable(1, 1);
    while(1);
}

void update_storage() {

  Serial.printf("TEST: ssid : %s",writecred.ssid);
  Serial.printf("TEST: ssid : %s",writecred.pwd);
  Serial.println("FLASH_PAGE_SIZE = " + String(FLASH_PAGE_SIZE, DEC));
  Serial.println("FLASH_SECTOR_SIZE = " + String(FLASH_SECTOR_SIZE,DEC));
  Serial.println("FLASH_BLOCK_SIZE = " + String(FLASH_BLOCK_SIZE, DEC));
  Serial.println("PICO_FLASH_SIZE_BYTES = " + String(PICO_FLASH_SIZE_BYTES, DEC));
  Serial.println("XIP_BASE = 0x" + String(XIP_BASE, HEX));
  first_empty_page = -1;
  
  // Read the flash using memory-mapped addresses
  // For that we must skip over the XIP_BASE worth of RAM
  // int addr = FLASH_TARGET_OFFSET + XIP_BASE;
  for(page = 0; page < FLASH_SECTOR_SIZE/FLASH_PAGE_SIZE; page++){
    addr = XIP_BASE + FLASH_TARGET_OFFSET + (page * FLASH_PAGE_SIZE);

    p = (int *)addr;
    write_cred_pt = (struct pageData *)addr;

    Serial.println("\nfirst_empty_page #" + String(first_empty_page, DEC));
    if( *p == -1 && first_empty_page < 0){
      first_empty_page = page;
      Serial.println("First empty page is #" + String(first_empty_page, DEC));

    }
  }

  if (first_empty_page < 0){
    Serial.println("Full sector, erasing...");
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    first_empty_page = 0;
    restore_interrupts (ints);
  }
  Serial.println("Writing to page #" + String(first_empty_page, DEC));
  uint32_t ints = save_and_disable_interrupts();
  flash_range_program(FLASH_TARGET_OFFSET + (first_empty_page*FLASH_PAGE_SIZE), (uint8_t *)&writecred, FLASH_PAGE_SIZE);
  restore_interrupts (ints);
}

void get_storage() {

  first_empty_page = -1;
  
  // Read the flash using memory-mapped addresses
  // For that we must skip over the XIP_BASE worth of RAM
  // int addr = FLASH_TARGET_OFFSET + XIP_BASE;
  for(page = 0; page < FLASH_SECTOR_SIZE/FLASH_PAGE_SIZE; page++){
    addr = XIP_BASE + FLASH_TARGET_OFFSET + (page * FLASH_PAGE_SIZE);

    p = (int *)addr; // this pointer is needed to check the last page with no data

    if( *p == -1 && first_empty_page < 0){
      first_empty_page = page;

      addr = XIP_BASE + FLASH_TARGET_OFFSET + ((page-1) * FLASH_PAGE_SIZE); // move back one page to get the latest data, as the ptr currently points to page with no data
      read_cred_pt = (struct pageData *)addr;
      
      break;
    }
  }


}


void init_stnmode() {

 	Serial.println("OLED intialized");

 	display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS); // Address 0x3C for 128x32
 	display.display();
 	delay(100);
 	display.clearDisplay(); // Clear the buffer.
 	display.display();
 	display.setTextSize(1);
 	display.setTextColor(WHITE);   

  get_storage();
  setup_wifi();
  ThingSpeak.begin(picoClient);
  dht.begin();
  Serial.println(F("Pico weather station !"));
     
  unsigned status;
  status = bmp.begin(BMP280_ADDRESS);
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    Serial.print("SensorID was: 0x"); Serial.println(bmp.sensorID(),16);
    while (1) delay(10);
  }
}


void init_apmode() { 
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);  // Configure static IP
   
  //Setting the AP Mode with SSID, Password, and Max Connection Limit
  if(WiFi.softAP(ap_ssid,ap_password,1,false,max_connections)==true)
  {
    Serial.print("Access Point is Created with SSID: ");
    Serial.println(ap_ssid);
    Serial.print("Max Connections Allowed: ");
    Serial.println(max_connections);
    Serial.print("Access Point IP: ");
    Serial.println(WiFi.softAPIP());
  }
  else
  {
    Serial.println("Unable to Create Access Point");
  }
 
  //Specifying the functions which will be executed upon corresponding GET request from the client
  server.on("/",HTTP_GET, handle_onconnect);
  server.on("/update", handle_update);
  server.on("/restart",handle_restart);
  server.onNotFound(handle_notfound);
  
  server.begin(); //Starting the Server
  Serial.println("HTTP Server Started");

}


void loop_stnmode() {

  float p = bmp.readPressure();
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  Serial.print(F("Pressure = "));
  Serial.print(p);
  Serial.print(" Pa ");
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(" % ");
  Serial.print(F("Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));

  ThingSpeak.setField(1, t);
  ThingSpeak.setField(2, h);
  ThingSpeak.setField(3, p);

  int rc = ThingSpeak.writeFields(CHANNEL_ID, CHANNEL_API_KEY);
  if (rc == 200){
    Serial.printf("mqtt publish successful !\n");
  } else {
    Serial.printf("mqtt call failed, return code :%i", rc);
  }

 			display.clearDisplay();
 			display.setCursor(0, 0);
 			display.println("   Weather Station");
 			display.print("Temperature: ");
 			display.print(t);
 			display.println(" C");
 			display.print("Atm Prsr(Pa):");
 			display.println(p);
 			display.print("Humidity:    ");
 			display.print(h);
 			display.println(" %");
 			display.display(); // refresh screen

  delay(DELAY_LOOP);
}


void loop_apmode() {

  server.handleClient(); //Assign the server to handle the clients
     
  //Continuously check how many stations are connected to Soft AP and notify whenever a new station is connected or disconnected
  new_stations=WiFi.softAPgetStationNum();
   
  if(current_stations<new_stations) //Device is Connected
  {
    current_stations=new_stations;
    Serial.print("New Device Connected to SoftAP... Total Connections: ");
    Serial.println(current_stations);
  }
   
  if(current_stations>new_stations) //Device is Disconnected
  {
    current_stations=new_stations;
    Serial.print("Device disconnected from SoftAP... Total Connections: ");
    Serial.println(current_stations);
  }
}


void setup_wifi() {

  delay(10);
  display.clearDisplay();
  display.setCursor(0, 0);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(read_cred_pt->ssid);

  display.println("Connecting to ");
  display.print(read_cred_pt->ssid);
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(read_cred_pt->ssid, read_cred_pt->pwd);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();

  }
  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  display.println("WiFi connected");

}

