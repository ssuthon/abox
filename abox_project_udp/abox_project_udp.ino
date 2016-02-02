 #include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>  
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include "LedControl.h"
#include <Time.h>
#include <TimeAlarms.h>
#include <DHT.h>

////////////////////////////////////////////define BOX number///////////////////////////////
//#define MAJOR_NO XX_MAJOR_NO_XX 
//#define MINOR_NO XX_MINOR_NO_XX 
#define MAJOR_NO 0
#define MINOR_NO 10
/////////////////////////////////////////////////////////////////////////////////////////

#define I2C_ADDR 0x27 //i2c scanner address
#define BACKLIGHT_PIN 3 //set up blacklight pin

#define CMD_PREFIX_LEN 3
#define RESPONSE_LEN 3
#define REGISTRAR_PORT 9183
#define BOX_SIGNAL_PORT 6000
/*
 pin 12 is connected to the DataIn 
 pin 11 is connected to the CLK 
 pin 10 is connected to LOAD 
 */  
LedControl lc = LedControl(12,11,10,1);
IPAddress ip(10, 0, MAJOR_NO, MINOR_NO);
IPAddress registrarIp(10, 0, 0, 1);
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, MAJOR_NO, MINOR_NO };
  

EthernetServer server(BOX_SIGNAL_PORT);
EthernetClient activeClient;
EthernetUDP udp;

IPAddress serialForwardAddress;
int serialForwardPort = -1;

DHT dht(8, DHT21);
LiquidCrystal_I2C lcd(I2C_ADDR,2,1,0,4,5,6,7);
AlarmId displayBoxAlarmId;
AlarmId updateSensorsAlarmId;
//void(* resetFunc) (void) = 0;//declare reset function at address 0

void setup() {
  Serial.begin(9600); 
  Serial1.begin(9600);
  Serial2.begin(9600);
   // initialize the ethernet device
  Ethernet.begin(mac, ip);
  //sensor part
  Wire.begin();
  // start listening for clients
  server.begin();
  udp.begin(6000);
  
  //Serial.println("Server Ready");
  lcd.begin (20,4);
  // Switch on the backlight
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.home ();
  lcd.setCursor(8,0);
  lcd.print("CMC");
  
  initLed();
  
  dht.begin();
 
  initAlarm(); 
}

void initLed(){
  //set LED 8x8
  lc.shutdown(0,false);
  lc.setIntensity(0,2);
  lc.clearDisplay(0);
}

void initAlarm(){
  displayBoxAlarmId = Alarm.timerRepeat(1, displayBoxInfo);
  Alarm.delay(191);
  updateSensorsAlarmId = Alarm.timerRepeat(17, updateSensors);
}

char cmd_buf[256];
int cmd_index = 0; 
unsigned long rfid_stamp = 0;
unsigned long serial2_stamp = 0;
float current_rh = 0;
float current_tp = 0;

void loop() {
  EthernetClient client1 = server.available();
  
  if(client1 && activeClient != client1){
    activeClient.stop();
    activeClient = client1;
  }
  
  int done = 0;
  while(activeClient.available() > 0 && !done) {
    done = processCommand(activeClient.read());
  }

  if(activeClient && !activeClient.connected()){
    activeClient.stop();
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
    
    char *cmd_method = trimwhitespace(cmd_buf);
    char *cmd_spec;
    if(cmd_method[2] != '_'){
      //Serial.println("invalid command");
      return 1;
    }
    cmd_method[2] = '\0';
    cmd_spec = cmd_method + CMD_PREFIX_LEN;

    char *response = (char*)malloc(128);
    char *response_body = response + RESPONSE_LEN + CMD_PREFIX_LEN;
    strcpy(response, "OK_");
    sprintf(response + RESPONSE_LEN, "%s_", cmd_method);
    
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
         setSerialForward(cmd_spec);
         succeeded = 1;
         break;
         
       case 'P':  //ping
         succeeded = 1;
         break;
       
       //case 'H': //hardware
         //if(cmd_method[1] == 'R')
           //resetFunc();
    }
    if(succeeded && activeClient.connected()){
      int end = strlen(response);
      strcpy(response + end, "\r\n");
      activeClient.print(response);    
    } 

    free(response);
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

void setSerialForward(char *spec){
  int len = strlen(spec);
  int count = 0;
  int c_index = 0;
  int last_dot_index = -1;
  while(count < 4 && c_index < len){
    if(spec[c_index] == '.' || spec[c_index] == ':'){
      spec[c_index] = '\0';
      serialForwardAddress[count] = atoi(spec + last_dot_index + 1);
      last_dot_index = c_index;
      count++;
    }
    c_index++;
  }

  if(c_index < len){
    serialForwardPort = atoi(spec + c_index);
  }else{
    serialForwardPort = -1;
  }
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

  if(*spec == '\0'){
    initLed();
    return;
  }
  //char limit 16, default '0'
  prepareSpec(spec, 16, '0');

  for(i = 0; i < 8; i++){
    v = hexstr2b(spec[i*2], spec[i*2 + 1]);
    lc.setRow(0, i, v);
  }
}

void readSensors(char *body){
  sprintf(body, "RH=%d.%d,TC=%d.%d", (int)current_rh, ((int)(current_rh *10)) % 10, 
    (int)current_tp, ((int)(current_tp *10)) % 10);
}

void forwardUdpData(byte b[], int len, int channel){
  if(activeClient.connected() && serialForwardPort > 0){
    udp.beginPacket(serialForwardAddress, serialForwardPort + channel);
    udp.write(b, len);
    udp.endPacket();
  }
}

byte code[12];  //12 + 4 (RF_XXXXXXXXXXXX\0)
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
  byte checksum = hexstr2b(code[10], code[11]);
  byte test = hexstr2b(code[0], code[1]);
  int i;
  for(i = 1; i < 5; i++){
    test ^= hexstr2b(code[i * 2], code[i*2 + 1]);
  }
  if(test == checksum){ //valid tag code
    code[10] = '\r';
    code[11] = '\n';
    
    rfid_stamp = millis();
    forwardUdpData(code, 12, 0);
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
     
     forwardUdpData(rs_buf, rsi, 1);
     rsi = 0;
   }else if(rsi >= 128){
     rsi = 0;  //discard message longer than 128 bytes
   }  
}

void updateSensors(){
  float rh = dht.readHumidity(); 
  float tp = dht.readTemperature();
  if (!isnan(rh) && !isnan(tp)) {
    current_rh = rh; 
    current_tp = tp;
  }
}

byte heartToggle = 0;
byte testTag[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\r', '\n'};
byte matrixLedOn = 0xFF;
void displayBoxInfo(){
    char *str = (char*)malloc(21);
    unsigned long now = millis();
    
    sprintf(str, "%d%03d %02d%02d %c%c%c  %02d%c%02d", ip[2], ip[3], 
      current_rh > 99 ? 99 : (int)(current_rh),
      current_tp > 99 ? 99 : (int)(current_tp),
      (now -serial2_stamp) < 2000 ? 'S' : ' ',
      activeClient.connected() ? 'N' : ' ',  
      (now - rfid_stamp) < 2000 ? 'R' : ' ',  
      hour(), heartToggle ? ':' : ' ', minute());
    displayTextLcd(4, str);
    free(str);
    
    heartToggle = !heartToggle;
    
    //blink matrix led
    if(!activeClient.connected()){
      for(int i = 0; i < 8; i++){
        lc.setRow(0, i, matrixLedOn);
      }    
      matrixLedOn = ~matrixLedOn;
      if(heartToggle){
        reportToRegistrar();
      }
    }
}

void reportToRegistrar(){
  udp.beginPacket(registrarIp, REGISTRAR_PORT);
  udp.write("Hi");
  udp.endPacket();
}

//------utilities functions-------------
byte hexstr2b(char a, char b){
  return (byte)((hexchar2b(a) * 16) + hexchar2b(b));
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
