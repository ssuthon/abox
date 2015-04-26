// this version is include LED 8x8 , RFID , Scale , LCD , sensor , with enc28j60
/*enc28j60 ethernet module
  pin 50 is connected to SO
  pin 51 is connected to SI
  pin 52 is connected to SCK
  pin 53 is connected to CS
  other 4 pin is not necessary
*/

#include <SPI.h>
#include <UIPEthernet.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include "LedControlMS.h"
#include <SHT2x.h>

#define I2C_ADDR 0x27 //i2c scanner address
#define BACKLIGHT_PIN 3 //set up blacklight pin

/*
 pin 12 is connected to the DataIn 
 pin 11 is connected to the CLK 
 pin 10 is connected to LOAD 
 */
LedControl lc=LedControl(12,11,10,1);
// network configuration.  gateway and subnet are optional.

 // the media access control (ethernet hardware) address for the shield:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  
//the IP address for the shield:
byte ip[] = { 192, 168, 1, 60 };    
// the router's gateway address:
byte gateway[] = { 192, 168, 1, 1 };
// the subnet:
byte subnet[] = { 255, 255, 255, 0 };

UIPServer server = UIPServer(6000);

LiquidCrystal_I2C lcd(I2C_ADDR,2,1,0,4,5,6,7);

void setup() {
  Serial.begin(9600); 
  Serial1.begin(9600);
  Serial2.begin(9600);
   // initialize the ethernet device
  Ethernet.begin(mac, ip, gateway, subnet);
  //sensor part
  Wire.begin();
  // start listening for clients
  server.begin();
  Serial.println("Server Ready");
  lcd.begin (20,4);
  // Switch on the backlight
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.home ();
  lcd.setCursor(8,0);
  lcd.print("CMC");
  
  //set LED 8x8
  lc.shutdown(0,false);
  lc.setIntensity(0,2);
  lc.clearDisplay(0);
  
}

void prepareSpec(char *spec, int limit, char dv){
  int len = strlen(spec);
  int i;
  for(i = len; i < limit; i++){
    spec[i] = dv;
  }
  spec[limit] = '\0';
}

void displayTextLcd(int line, char *spec){  
  //char limit 20, default space
  prepareSpec(spec, 20, ' ');
  
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.setCursor(0, line - 1);
  lcd.print(spec);
}

void displayMatrixLed(char *spec){
  byte v;
  int i;

  //char limit 16, default '0'
  prepareSpec(spec, 16, '0');
 
  for(i = 0; i < 8; i++){
    v = hexstr2b(spec + (i*2));
    lc.setRow(0, i, v);
  }
}

void readSensors(char *body){
  float rh = SHT2x.GetHumidity();
  float tc = SHT2x.GetTemperature();
  Serial.print("Humidity(%RH): ");
  Serial.println(rh);
  Serial.print("Temperature(C): ");
  Serial.println(tc);
  
  sprintf(body, "RH=%.2f,TC=%.2f", rh, tc);
}

char cmd_buf[256];
char response[128];
char response_body[120];
char tmp;
int cmd_index = 0;
char *cmd_method;
char *cmd_spec;
int line;
int succeeded;  
int serial2_forward = 1;

void loop() {
  // if an incoming client connects, there will be bytes available to read:
  UIPClient client = server.available();
  if(client) {
    tmp = client.read();
    if(tmp != '\n'){
      cmd_buf[cmd_index++] = tmp;
      return;
    }else{
      cmd_buf[cmd_index] = '\0';
      cmd_index = 0;      
    }
    
    cmd_method = trimwhitespace(cmd_buf);
    if(cmd_method[2] != '_'){
      Serial.println("invalid command");
      return;
    }
    
    cmd_method[2] = '\0';
    cmd_spec = cmd_method + 3;
    response_body[0] = '\0';
    
    switch(cmd_method[0]){
       case 'T':  //text lcd
         line = cmd_method[1] - '0';
         displayTextLcd(line, cmd_spec);
         succeeded = 1;
         break;
      
       case 'M':  //matrix led 
         displayMatrixLed(cmd_spec);
         succeeded = 1;
         break;
         
       case 'S':  //sensor
         //readSensors(response_body);
         succeeded = 1;
         break;
         
       case 'R':  //rs232
         serial2_forward = (cmd_method[1] == '0') ? 0 : 1;
         succeeded = 1;
         break;
      
    }
    if(succeeded){
      sprintf(response, "OK_%s_%s\r\n", cmd_method, response_body);
      server.print(response);    
    }
  }   
}

char code[12];
int ci = 0;
void serialEvent1(){
  
  byte bytesRead = 0;
  byte val = 0;
 
  char rf[10] = "RF_";
  while(Serial1.available() > 0 && bytesRead < 16) { //make sure while not run indefinitely
    val = Serial1.read();
    bytesRead++;
    
    if(val == 0x02){
      ci = 0;
    }else if(val == 0x0D || val == 0x0A){ //ignore \r\n
      continue;
    }else if(val == 0x03){
      process_code();
      break;
    }else if(ci < 12) {
      code[ci++] = val;
    }
  }
}

void process_code() {
  byte checksum = hexstr2b(code + 10);
  byte test = hexstr2b(code);
  int i;
  for(i = 1; i < 5; i++){
    test ^= hexstr2b(code + (i * 2));
  }
  if(test == checksum){ //valid tag code
    code[10] = '\0';
    Serial.println(code);
    server.print("RF_");
    server.print(code);
    server.print("\r\n");
  }
}

char rs_buf[128];
int rsi = 0;
void serialEvent2() { 
  int ok = 0;
   while(Serial2.available() > 0 && rsi < 127 && !ok){
     byte v = Serial2.read();
     rs_buf[rsi++] = v;
     if(v == 0x0D){
       ok = 1;
     }
   }

   if(rsi > 0 && ok){
     rs_buf[rsi] = '\0';
     if(serial2_forward){
       server.print("SR_");
       server.print(trimwhitespace(rs_buf));
       server.print("\r\n");
     }
     rsi = 0;
   }else if(rsi > 127){
     rsi = 0;  //discard message longer than 127 char
   }  
}

byte hexstr2b(char *s){
  return (byte)((hexchar2b(s[0]) * 16) + hexchar2b(s[1]));
}

byte hexchar2b(char hex){
  if(hex >= '0' && hex <= '9')
    return hex -'0';
  if(hex >= 'A' && hex <= 'F'){     
    return 10 + (hex - 'A');  
  }
}

char *trimwhitespace(char *str) {
  char *end;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}
