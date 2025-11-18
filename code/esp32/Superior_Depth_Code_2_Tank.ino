unsigned char buffer_RTT[4] = {0};
uint8_t CS;


//defines pins and BAUD rate
#define COM 0x55
#define RXD2 16
#define TXD2 17
#define BAUD 115200

//intializes class
HardwareSerial sillySerial(2);


int Distance = 0;
void setup() {

  Serial.begin(115200);
  sillySerial.begin(BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial 2 started at 9600 baud rate");

}
void loop() {
  sillySerial.write(COM);
  delay(100);

  //code will print available followed by the depth measurement 

  if(sillySerial.available() > 0){
    Serial.println("available");
    delay(4);
    
    if(sillySerial.read() == 0xff){    
      buffer_RTT[0] = 0xff;

      for (int i=1; i<4; i++){
        buffer_RTT[i] = sillySerial.read();
      }
      CS = buffer_RTT[0] + buffer_RTT[1]+ buffer_RTT[2];  

      if(buffer_RTT[3] == CS) {
        Distance = (buffer_RTT[1] << 8) + buffer_RTT[2];
        Serial.print("Distance:");
        Serial.print(Distance);
        Serial.println("mm");
      }
    }
  }
  else{
    Serial.println("not available");
  }
}
