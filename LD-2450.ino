#include <Arduino.h>

#define RX_PIN 16
#define TX_PIN 17

byte startMarkers[] = {0xF4, 0xF3, 0xF2, 0xF1 }; //Expected response from radar.
int messageSize = 27;
uint8_t radarData[27];
char data[20]; //response message buffer
int dataSize = 20;
int returnValue = 0;

unsigned long timeout = 1000; //one second timeout on response receive

char startConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 4, 3, 2, 1}; //command header, data, trailer
char endConfig[] =   {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 4, 3, 2, 1}; //command header, data, trailer
char Settings1[] =   {0xFD, 0xFC, 0xFB, 0xFA, 0x06, 0x00, 0x02, 0x00, 0x05, 0x02, 0x05, 0x02, 4, 3, 2, 1}; //command header, data, trailer
char Settings2[] =   {0xFD, 0xFC, 0xFB, 0xFA, 0x06, 0x00, 0x03, 0x00, 0x01, 0x03, 0x00, 0x00, 4, 3, 2, 1}; //command header, data, trailer
char Targets[] =     {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0x12, 0x00, 4, 3, 2, 1};
char Sensitivity[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0x13, 0x00, 4, 3, 2, 1};
char Firmware[] =    {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xA0, 0x00, 4, 3, 2, 1};

char Reset[] =       {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xA2, 0x00, 4, 3, 2, 1};
char Restart[] =     {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xA3, 0x00, 4, 3, 2, 1};
char BluetoothOn[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xA4, 0x00, 0x01, 0x00, 4, 3, 2, 1}; //from HLK-LD2410B serial protocol manual
char BluetoothOff[] ={0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xA4, 0x00, 0x00, 0x00, 4, 3, 2, 1};

  void setup() {
  Serial.begin(115200);                  
  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  //Serial2.setRxBufferSize(54);           // Set buffer size
  delay(200);
  Serial.println("HLK-LD2451 Radar Module Initialized");

  
  Serial.println("Set config mode");
  Serial2.write(startConfig, sizeof startConfig);
  delay(10);
  returnValue = getResponse();
  Serial.println((returnValue == 0) ? "OK" : "Failed");
  delay(10);
/*
  Serial.println("Receive firmware version from radar");
  Serial2.write(Firmware, sizeof Firmware);
  delay(10);
  returnValue = getResponse();
  Serial.println((returnValue == 0) ? "OK" : "Failed");
  delay(10);
*/
  Serial.println("Receive motion sensitivity from each range gates");
  Serial2.write(Sensitivity, sizeof Sensitivity);
  delay(10);
  returnValue = getResponse();
  Serial.println((returnValue == 0) ? "OK" : "Failed");
  delay(10);

  Serial.println("Settings for distance, approach, Minimum speed and no target delay");
  Settings1[8] =  10;  //Maximum detection distance in meters
  Settings1[9] =  02;  //Approch,away or both ways trafic (0= away only, 01= approch only and 02 = both ways traffic)
  Settings1[10] = 01;  //Minimum detectable speed in km/h (0 to 78 (100 km/h))
  Settings1[11] = 00;  //No target delay in seconds (0 to FF)
  Serial2.write(Settings1, sizeof Settings1);
  delay(10);
  returnValue = getResponse();
  Serial.println((returnValue == 0) ? "OK" : "Failed");
  delay(10);
  
  Serial.println("Settings for triggers and Signal to Noise Ratio (SNR)");
  Settings2[8] = 01; //Set number of triggers for alarm pin on sensor
  Settings2[9] = 03; //Set minimimum SNR (3 to 8 - Lower number 3 is most sensetive)
  Serial2.write(Settings2, sizeof Settings2);
  delay(10);
  returnValue = getResponse();
  Serial.println((returnValue == 0) ? "OK" : "Failed");
  delay(10);
/*
  //Bluetooth off
  Serial.println("BT off");
  Serial2.write(BluetoothOff, sizeof BluetoothOff);
  delay(10);
  returnValue = getResponse();
  Serial.println((returnValue == 0) ? "OK" : "Failed");

  delay(10000); //check for BT off

  //Bluetooth on
  Serial.println("BT on");
  Serial2.write(BluetoothOn, sizeof BluetoothOn);
  delay(10);
  returnValue = getResponse();
  Serial.println((returnValue == 0) ? "OK" : "Failed");
*/
  Serial2.write(endConfig, sizeof endConfig);
  delay(100);
  returnValue = getResponse();
  Serial.println((returnValue == 0) ? "OK" : "Failed");
  Serial.println("Done with configuration");
/*
  //Reboot the radar
  Serial.println("Reboot the radar to apply new config settings");
  Serial2.write(Restart, sizeof Restart);
  delay(1000);
*/
}

void loop()
 {
  char string[32];


 // int availableBytes = Serial2.available();
  for(int i=0; i < messageSize; i++)
    {
     string[i] = Serial2.read();
    }
      for (int i = 0; i < messageSize ; i++) {

       radarData[i] = uint8_t(string[i]);
       Serial.printf("%i ",  radarData[i]); -Serial.println();
       
      } 
          
     delay(500)     
          
 }

int getResponse() {
 
 unsigned long startTime = millis();
 int indx = 0, j = 0;
 char c;
 int state = 0; //waiting for data
 
 int count = 0;
 
 while (millis() - startTime < timeout) {
    if (Serial2.available()) {
      c = Serial2.read();
     /* 
      Serial.print(c, DEC); 
      Serial.print(' '); 
     */
      switch (state) {

        case 0:
          if (c != startConfig[count])
          { //header doesn't match
            return 1;
          }
          else {
            count++;
            if (count > 3) //have the 4 message startbytes.
              state = 1; //get message size
            indx = 0;
          }
          break;

        case 1:  //get message size
          messageSize = c; //The next 2 bytes tells the size in bytes of the dataframe that follows
          state = 2;
          break;

        case 2:  //high byte of size
          messageSize += (c << 8);
          state = 3;
          if (messageSize == 0) state = 4; //no message
          break;

        case 3:  //get data, if any
          if (indx < dataSize) data[indx] = c;  //buffer overflow check
          indx++;
          messageSize--;
          if (messageSize == 0)
            state = 4;  //get frame end markers
          break;

        case 4:
          count--; //ignore four end markers
          if (count == 0) {
            Serial.println(); 
            return data[2]; //low byte of second data word, 0=OK, 1=failure
          }
      }
    }
  }//end while timeout
  Serial.println("response timeout");
  return 1;
}
