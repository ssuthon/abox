
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);
IPAddress target(192, 168, 1, 1);
EthernetUDP Udp;

unsigned int localPort = 8888;      // local port to listen on
unsigned int remotePort = 8888;

void setup() {
  // put your setup code here, to run once:
   Ethernet.begin(mac, ip);
   Udp.begin(localPort);
   Serial2.begin(9600);

}
 
void loop() {
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
     Udp.beginPacket(target, remotePort);
     Udp.write(rs_buf, rsi);
     Udp.endPacket();

     rsi = 0;
   }else if(rsi >= 128){
     rsi = 0;  //discard message longer than 128 bytes
   }  
}

