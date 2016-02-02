#include <Ethernet.h>
#include <EthernetUdp.h>  

#define BOX_SIGNAL_PORT 6000
#define REGISTRAR_PORT 9183

#define MAX_COUNT 8

IPAddress ip(10, 0, 0, 1);
byte mac[] = { 0xDF, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };

EthernetClient client;
EthernetUDP Udp;

void setup() {
  Ethernet.begin(mac,ip);
  Udp.begin(REGISTRAR_PORT);
  Serial1.begin(9600);
}

int count = 0;
char buf[128];
char mx_buf[][17] = {
  "00000000000000FF",
  "000000000000FFFF",
  "0000000000FFFFFF",
  "00000000FFFFFFFF",
  "000000FFFFFFFFFF",
  "0000FFFFFFFFFFFF",
  "00FFFFFFFFFFFFFF",
  "FFFFFFFFFFFFFFFF"
};

void loop() {
  // put your main code here, to run repeatedly:
  if(!client.connected()){
    int packetSize = Udp.parsePacket();
    if(packetSize) {
      IPAddress remote = Udp.remoteIP();
      client.connect(remote, BOX_SIGNAL_PORT);
    }
  }else{
    sprintf(buf, "T1_%d", count);    
    client.println(buf);
    client.flush();    

    delay(50);

    sprintf(buf, "MX_%s", mx_buf[count]);    
    client.println(buf);
    client.flush();    

    count++;    

    count = count % MAX_COUNT;
  }
  Serial1.println("OK");
  
  delay(1000);
}
