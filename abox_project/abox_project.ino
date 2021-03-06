#include <SPI.h>
#include <UIPEthernet.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include "LedControlMS.h"
#include <Time.h>
#include <TimeAlarms.h>
#include <DHT.h>

#define I2C_ADDR 0x27 //i2c scanner address
#define BACKLIGHT_PIN 3 //set up blacklight pin

/*
 pin 12 is connected to the DataIn 
 pin 11 is connected to the CLK 
 pin 10 is connected to LOAD 
 */
LedControl lc = LedControl(12,11,10,1);
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192, 168, 1, 60 };    

EthernetServer server1(6000);
EthernetClient activeClient1;
EthernetServer server2(6001);
EthernetClient activeClient2;

DHT dht(8, DHT21);
LiquidCrystal_I2C lcd(I2C_ADDR,2,1,0,4,5,6,7);
AlarmId displayBoxAlarmId;
AlarmId updateSensorsAlarmId;

void setup() {
  Serial.begin(9600); 
  Serial1.begin(9600);
  Serial2.begin(9600);
   // initialize the ethernet device
  Ethernet.begin(mac, ip);
  //sensor part
  Wire.begin();
  // start listening for clients
  server1.begin();
  server2.begin();
  
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
  
  dht.begin();
 
  initAlarm(); 
}

void initAlarm(){
  displayBoxAlarmId = Alarm.timerRepeat(2, displayBoxInfo);
  Alarm.delay(191);
  updateSensorsAlarmId = Alarm.timerRepeat(17, updateSensors);
}

char cmd_buf[256];
char response[128];
char response_body[120];
int cmd_index = 0; 
byte serial2_forward = 1;
unsigned long rfid_stamp = 0;
unsigned long serial2_stamp = 0;
float current_rh = 0;
float current_tp = 0;

void loop() {
  EthernetClient client1 = server1.available();
  EthernetClient client2 = server2.available();
  
  if(client1 && activeClient1 != client1){
    activeClient1.flush();
    activeClient1.stop();
    activeClient1 = client1;
  }
    
  if(client2 && activeClient2 != client2){
    activeClient2.flush();
    activeClient2.stop();
    activeClient2 = client2;
    activeClient2.flush();
  }
  
  int done = 0;
  while(activeClient1.available() > 0 && !done) {
    done = processCommand(activeClient1.read());
  }
  
  while(activeClient2.available() > 0){
    activeClient2.read();
  }
  
  if(activeClient1 && !activeClient1.connected()){
    activeClient1.stop();
  }
  if(activeClient2 && !activeClient2.connected()){
    activeClient2.stop();
  }  
  
  Alarm.delay(0);
}

int processCommand(char tmp){
  int succeeded = 0;
  
   if(tmp != '\n'){
      cmd_buf[cmd_index++] = tmp;
      return 0;
    }else{
      cmd_buf[cmd_index] = '\0';
      cmd_index = 0;      
    }
    
    char *cmd_method;
    char *cmd_spec;
    
    cmd_method = trimwhitespace(cmd_buf);
    if(cmd_method[2] != '_'){
      Serial.println("invalid command");
      return 1;
    }
    
    cmd_method[2] = '\0';
    cmd_spec = cmd_method + 3;
    response_body[0] = '\0';
    
    switch(cmd_method[0]){
       case 'T':  //text lcd
         displayTextLcd(cmd_method[1] - '0', cmd_spec);
         succeeded = 1;
         break;
      
       case 'M':  //matrix led 
         displayMatrixLed(cmd_spec);
         succeeded = 1;
         break;
         
       case 'S':  //sensor
         readSensors(response_body);
         succeeded = 1;
         break;
         
       case 'C':  //clock
         setCurrentTime(cmd_spec);
         succeeded = 1;
         break;
         
       case 'R':  //rs232
         serial2_forward = (cmd_method[1] == '0') ? 0 : 1;
         succeeded = 1;
         break;
         
       case 'P':  //ping
         succeeded = 1;
         break;
    }
    if(succeeded && activeClient1.connected()){
      sprintf(response, "OK_%s_%s\r\n", cmd_method, response_body);
      activeClient1.print(response);    
    } 
    return 1;
}

void prepareSpec(char *spec, int limit, char dv){
  int len = strlen(spec);
  int i;
  for(i = len; i < limit; i++){
    spec[i] = dv;
  }
  spec[limit] = '\0';
}

void setCurrentTime(char *spec){
  Alarm.free(displayBoxAlarmId);
  Alarm.free(updateSensorsAlarmId);
  
  spec[2] = '\0';
  spec[5] = '\0';
  spec[8] = '\0';
  setTime(atoi(spec), atoi(spec+3), atoi(spec+6), 0, 0, 0);
  
  initAlarm();
}

void displayTextLcd(int line, char *spec){  
  //char limit 20, default space
  prepareSpec(spec, 20, ' ');
  
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
  sprintf(body, "RH=%d.%d,TC=%d.%d", (int)current_rh, ((int)(current_rh *10)) % 10, 
    (int)current_tp, ((int)(current_tp *10)) % 10);
}

char code[12];
int ci = 0;
void serialEvent1(){
  
  byte bytesRead = 0;
  byte val = 0;
 
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
    rfid_stamp = millis();
    if(activeClient1.connected()){
      activeClient1.print("RF_");
      activeClient1.print(code);
      activeClient1.print("\r\n");
    }
  }
}

byte rs_buf[128];
int rsi = 0;
void serialEvent2() { 
  int ok = 0;
   while(Serial2.available() > 0 && rsi < 128 && !ok){
     byte v = Serial2.read();
     rs_buf[rsi++] = v;
     if(v == 0x0D){
       ok = 1;
     }
   }

   if(rsi > 0 && ok){
     serial2_stamp = millis();
     if(activeClient2.connected() && serial2_forward){
       activeClient2.write(rs_buf, rsi);
       activeClient2.flush();
     }
     rsi = 0;
   }else if(rsi >= 128){
     rsi = 0;  //discard message longer than 128 bytes
   }  
}

/*void serialEvent2(){
  int max_bytes = 128;
  while(Serial2.available() > 0 && max_bytes-- > 0){
    byte v = Serial2.read();
    if(activeClient2.connected() && serial2_forward){
      activeClient2.write(v);
    }
  }
  serial2_stamp = millis();
}*/

void updateSensors(){
  float rh = dht.readHumidity(); 
  float tp = dht.readTemperature();
  if (!isnan(rh) && !isnan(tp)) {
    current_rh = rh; 
    current_tp = tp;
  }
}

byte heartToggle = 0;
void displayBoxInfo(){
    char *str = (char*)malloc(21);
    unsigned long now = millis();
    
    sprintf(str, "%d%03d %02d%02d %c%c%c  %02d%c%02d", ip[2], ip[3], 
      current_rh > 99 ? 99 : (int)(current_rh),
      current_tp > 99 ? 99 : (int)(current_tp),
      (now -serial2_stamp) < 2000 ? 'S' : ' ',
      activeClient2.connected() ? 'N' : ' ',  
      (now - rfid_stamp) < 2000 ? 'R' : ' ',  
      hour(), heartToggle ? ':' : ' ', minute());
    displayTextLcd(4, str);
    free(str);
    
    heartToggle = !heartToggle;
}

//------utilities functions-------------
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
