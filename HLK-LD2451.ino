#include <Arduino.h>
#include <SimpleKalmanFilter.h>             // Built in arduino library. Reference : https://github.com/denyssene/SimpleKalmanFilter
/*
 SimpleKalmanFilter(e_mea, e_est, q);
 e_mea: Measurement Uncertainty 
 e_est: Estimation Uncertainty 
 q: Process Noise
 */
SimpleKalmanFilter simpleKalmanFilter(2, 2, 0.01);

#define RX_PIN 16
#define TX_PIN 17
#define BAUD_RATE 256000

// Variables
uint8_t RX_BUF[64] = {0};
uint8_t RX_count = 0;
uint8_t RX_temp = 0;

// Target details
int16_t target1_x = 0, target1_y = 0;
int16_t target1_speed = 0;
uint16_t target1_distance_res = 0;
float target1_distance = 0;
float target1_angle =0;


void setup() {
    Serial.begin(115200);                  // Debugging
    Serial2.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial2.setRxBufferSize(64);           // Set buffer size
    Serial.println("HLK-LD2451 Radar Module Initialized");

    delay(200);
            
    RX_count = 0;
    Serial2.flush();
}


void loop() {
    // Read data from Serial2
    while (Serial2.available()) {
        RX_temp = Serial2.read();
        RX_BUF[RX_count++] = RX_temp;

        // Prevent buffer overflow
        if (RX_count >= sizeof(RX_BUF)) {
            RX_count = sizeof(RX_BUF) - 1;
        }

        // Check for end of frame (0xF5, 0xF6)
        if ((RX_count > 1) && (RX_BUF[RX_count - 1] == 0xF5) && (RX_BUF[RX_count - 2] == 0xF6)) {
            processRadarData();
        }
    }
}


void processRadarData() {

   Serial.println("Raw data received from radar: ");
    for (int i = 0; i < RX_count; i++) {
      Serial.print(RX_BUF[i], DEC);
      Serial.print(" ");
    }
    Serial.println();

   /* RX_BUF: 0xAA 0xFF 0x03 0x00                   Header
    *  0x05 0x01 0x19 0x82 0x00 0x00 0x68 0x01      target1
    *  0xE3 0x81 0x33 0x88 0x20 0x80 0x68 0x01      target2
    *  0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00      target3
    *  0x55 0xCC                                    check data


    This code extracts and calculates information about a target 
    from a data buffer (RX_BUF). 
    It combines two bytes to get the X and Y coordinates, then 
    adjusts them: the X coordinate is shifted by 0x200 to center
    it around zero, and the Y coordinate is shifted by 0x8000 to 
    handle negative values. The speed is calculated similarly, 
    adjusted by 0x10 for an offset. 
    The distance resolution is read as-is, while the total distance
    is found using the Pythagorean theorem to combine X and Y.
    Finally, the angle is calculated using the atan2 function, 
    converting the position into degrees for easier interpretation.
    
  */

  
    if (RX_count >= 27) {
        
        // Extract data for Target 1
     /*  
       
       // Code modification claimed by tetraquarkneutrino in original article comments.
       int16_t target1_x1 = (RX_BUF[4] | (RX_BUF[5] << 8));
       target1_x = target1_x1 & 0x7fff;
       if(!(target1_x1 & 0x8000)){
       target1_x = target1_x * -1;
       }

       int16_t target1_y1 = (RX_BUF[6] | (RX_BUF[7] << 8));
       target1_y = target1_y1 & 0x7fff;
       if(!(target1_y1 & 0x8000)){
       target1_y = target1_y * -1;
       }


       Comment by @MrD562 on youtube channel
       2 weeks ago
       I don't think that you calculated target1_speed correctly.
       This seems to work:

       target1_speed = (RX_BUF[8] | (RX_BUF[9] << 8));
        if (target1_speed < 0) {
          target1_speed = -(target1_speed + 32767);
        }

     */
        target1_x = (RX_BUF[4] | (RX_BUF[5] << 8)) - 0x200;
        target1_y = (RX_BUF[6] | (RX_BUF[7] << 8)) - 0x8000;
        target1_speed = (RX_BUF[8] | (RX_BUF[9] << 8)) - 0x10;
        target1_distance_res = (RX_BUF[10] | (RX_BUF[11] << 8));
        target1_distance = sqrt(pow(target1_x, 2) + pow(target1_y, 2));
/*        Serial.print("Target 1 - Distance: ");
        Serial.println(target1_distance / 10.0);
        target1_distance = simpleKalmanFilter.updateEstimate(target1_distance);
*/
        target1_angle = atan2(target1_y, target1_x) * 180.0 / PI;

        Serial.print("Target 1 - Distance: ");
        Serial.print(target1_distance / 10.0);
        Serial.print(" cm, Angle: ");
        Serial.print(target1_angle);
        Serial.print(" degrees, X: ");
        Serial.print(target1_x);
        Serial.print(" mm, Y: ");
        Serial.print(target1_y);
        Serial.print(" mm, Speed: ");
        Serial.print(target1_speed);
        Serial.print(" cm/s, Distance Resolution: ");
        Serial.print(target1_distance_res);
        Serial.println(" mm");

        /*
        // Extract data for Target 2
        target2_x = (RX_BUF[12] | (RX_BUF[13] &lt;&lt; 8)) - 0x200;
        target2_y = (RX_BUF[14] | (RX_BUF[15] &lt;&lt; 8)) - 0x8000;
        target2_speed = (RX_BUF[16] | (RX_BUF[17] &lt;&lt; 8)) - 0x10;
        target2_distance_res = (RX_BUF[18] | (RX_BUF[19] &lt;&lt; 8));
        float target2_distance = sqrt(pow(target2_x, 2) + pow(target2_y, 2));
        float target2_angle = atan2(target2_y, target2_x) * 180.0 / PI;

        Serial.print("Target 2 - Distance: ");
        Serial.print(target2_distance / 10.0);
        Serial.print(" cm, Angle: ");
        Serial.print(target2_angle);
        Serial.print(" degrees, X: ");
        Serial.print(target2_x);
        Serial.print(" mm, Y: ");
        Serial.print(target2_y);
        Serial.print(" mm, Speed: ");
        Serial.print(target2_speed);
        Serial.print(" cm/s, Distance Resolution: ");
        Serial.print(target2_distance_res);
        Serial.println(" mm");

        // Extract data for Target 3
        target3_x = (RX_BUF[20] | (RX_BUF[21] &lt;&lt; 8)) - 0x200;
        target3_y = (RX_BUF[22] | (RX_BUF[23] &lt;&lt; 8)) - 0x8000;
        target3_speed = (RX_BUF[24] | (RX_BUF[25] &lt;&lt; 8)) - 0x10;
        target3_distance_res = (RX_BUF[26] | (RX_BUF[27] &lt;&lt; 8));
        float target3_distance = sqrt(pow(target3_x, 2) + pow(target3_y, 2));
        float target3_angle = atan2(target3_y, target3_x) * 180.0 / PI;

        Serial.print("Target 3 - Distance: ");
        Serial.print(target3_distance / 10.0);
        Serial.print(" cm, Angle: ");
        Serial.print(target3_angle);
        Serial.print(" degrees, X: ");
        Serial.print(target3_x);
        Serial.print(" mm, Y: ");
        Serial.print(target3_y);
        Serial.print(" mm, Speed: ");
        Serial.print(target3_speed);
        Serial.print(" cm/s, Distance Resolution: ");
        Serial.print(target3_distance_res);
        Serial.println(" mm");
        */
        // Reset buffer and counter
        memset(RX_BUF, 0x00, sizeof(RX_BUF));
        RX_count = 0;
    }
}
