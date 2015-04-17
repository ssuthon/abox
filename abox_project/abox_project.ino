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

EthernetServer server = EthernetServer(6000);
EthernetServer server2 = EthernetServer(6001);
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
    v = (byte)((hexstr2i(spec[i * 2]) * 16) + hexstr2i(spec[i * 2 + 1]));
    lc.setRow(0, i, v);
  }
}

void readSensors(char *body){
  Serial.print("Humidity(%RH): ");
  Serial.println(SHT2x.GetHumidity());
  Serial.print("Temperature(C): ");
  Serial.println(SHT2x.GetTemperature());
  
  sprintf(body, "RH=%f&TC=%f", SHT2x.GetHumidity(), SHT2x.GetTemperature());
}

char cmd_buf[256];
char response[128];
char response_body[120];
  
void loop()
{
  char tmp;
  int i;

  int cmd_index = 0;
  char *cmd_method;
  char *cmd_spec;
  int line;
  
  int succeeded;

  // if an incoming client connects, there will be bytes available to read:
  EthernetClient client = server.available();
  EthernetClient client2 = server2.available();
  while (client) { 
    tmp = client.read();
    client = server.available();
    if(tmp != '\n'){
      cmd_buf[cmd_index++] = tmp;
      continue;
    }else{
      cmd_buf[cmd_index] = '\0';
      cmd_index = 0;      
    }
    
    cmd_method = trimwhitespace(cmd_buf);
    if(cmd_method[2] != '_'){
      Serial.println("invalid command");
      continue;
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
         readSensors(response_body);
         succeeded = 1;
         break;
      
    }
    if(succeeded){
      sprintf(response, "OK_%s_%s\r\n", cmd_method, response_body);
      if(!client){
        server.println(response);
      }
    }
  }   
}
void serialEvent1()
{
  byte i = 0;
  byte val = 0;
  byte code[6];
  byte checksum = 0;
  byte bytesread = 0;
  byte tempbyte = 0;
  //EthernetClient client = server.available();
  char rf[10] = "RF_";
  if(Serial1.available() > 0) {
    if((val = Serial1.read()) == 2) {                  // check for header 
      bytesread = 0; 
      while (bytesread < 12) {                        // read 10 digit code + 2 digit checksum
        if( Serial1.available() > 0) { 
          val = Serial1.read();
          if((val == 0x0D)||(val == 0x0A)||(val == 0x03)||(val == 0x02)) { // if header or stop bytes before the 10 digit reading 
            break;                                    // stop reading
          }

          // Do Ascii/Hex conversion:
          if ((val >= '0') && (val <= '9')) {
            val = val - '0';
          } else if ((val >= 'A') && (val <= 'F')) {
            val = 10 + val - 'A';
          }

          // Every two hex-digits, add byte to code:
          if (bytesread & 1 == 1) {
            // make some space for this hex-digit by
            // shifting the previous hex-digit with 4 bits to the left:
            code[bytesread >> 1] = (val | (tempbyte << 4));

            if (bytesread >> 1 != 5) {                // If we're at the checksum byte,
              checksum ^= code[bytesread >> 1];       // Calculate the checksum... (XOR)
            };
          } else {
            tempbyte = val;                           // Store the first hex digit first...
          };

          bytesread++;                                // ready to read next digit
        } 
      } 
    
      // Output to Serial:
      if (bytesread == 12) {                          // if 12 digit read is complete
        Serial.print("5-byte code: ");
        server.print(rf);
        for (i=0; i<5; i++) {
          if (code[i] < 16) {
             Serial.print("0");
             server.print("0");
          }
          Serial.print(code[i],HEX);
          server.print(code[i],HEX);
          Serial.print(" ");
        }
        server.println();
        Serial.println();

        Serial.print("Checksum: ");
        Serial.print(code[5], HEX);
        Serial.println(code[5] == checksum ? " -- passed." : " -- error.");
        Serial.println();
      }

      bytesread = 0;
    }
  }
}

void serialEvent2()
{
    byte buf[2048];
    int len;
    //EthernetClient client2;
      if(Serial2.available() > 0){
        len = Serial2.readBytesUntil('\n',buf,2047);
        buf[len++] = '\n';
        server2.write(buf,len);
      }
}

int hexstr2i(char hex){
  if(hex >= '0' && hex <= '9')
    return hex -'0';
  if(hex >= 'A' && hex <= 'F'){     
    return 10 + (hex - 'A');  
  }
}

char *trimwhitespace(char *str)
{
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
