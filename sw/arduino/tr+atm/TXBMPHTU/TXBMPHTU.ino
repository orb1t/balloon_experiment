#include <RHReliableDatagram.h>
#include <SPI.h>
#include <RH_RF22.h>
#include <Wire.h>
#include <SFE_BMP180.h>
#include "SparkFun_Si7021_Breakout_Library.h"

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

static const int SDN = 10;
double bmp180;
double htu21d;
//uint8_t packet[RH_RF22_MAX_MESSAGE_LEN];
uint8_t packet[12];
char status;
double T,P,p0,a;  
int Tb_i, P_i, a_i, Th_i, H_i;

//Create an instance of the object
SFE_BMP180 pressure;
Weather sensor;

// Singleton instance of the radio driver.
RH_RF22 driver(4,2);

// Class to manage message delivery and receipt, using the driver declared above.
RHReliableDatagram rf22(driver, CLIENT_ADDRESS);

void setup() {
    Wire.begin();        // Join i2c bus  
    Serial.begin(115200);
    pinMode(SDN, OUTPUT);
    digitalWrite(SDN, LOW);
    delay(1500);
    if (!rf22.init()) {
        Serial.println(F("Initialization failed"));
    }
    Serial.println(F("Unidirectional test (TX)"));
    driver.setCRCPolynomial(driver.CRC_CCITT);
    driver.setHeaderFlags(0x7E); 
    driver.setFrequency(437.225, 0.05); 
    driver.setTxPower(RH_RF22_TXPOW_20DBM);
    if (!driver.setModemConfig(driver.FSK_Rb2Fd5)) {
        Serial.println(F("Configuration error"));    
    }
    rf22.setRetries(3);
    Serial.println(F("Set Tx Power = RH_RF22_TXPOW_20DB"));
    Serial.println(F("Set configuration = FSK_Rb2Fd5"));  
    // BMP180
    pressure.begin();
}

void loop() {
    encodePacket();
    sendPacket(packet, sizeof(packet));
}

/**
 * This function sends a packet to the receiver and then it waits for a confirmation,
 * if there is no acknowledge, an error is printed on screen.
 * @param data[] Data sent to the receiver..
 * @param data_size Size of the packet to be sent.
 */
void sendPacket(uint8_t packet[], int packet_size) {
    if (!rf22.sendtoWait(packet, packet_size, SERVER_ADDRESS))
        Serial.println(F("sendtoWait failed"));
}

/**
 * The packet is encoded transforming data type from double or uint32_t to
 * uint8_t for compatibility with the transceiver. In order to not loose
 * infromation, the decimal numbers are multiplied with a factor and separated
 * in 2 different octets.
 */
void encodePacket() {
    status = pressure.startTemperature();
    delay(status);
    status = pressure.getTemperature(T);
    status = pressure.startPressure(3);
    delay(status);
    status = pressure.getPressure(P,T);
    p0 = 1013.25; 
    a = pressure.altitude(P,p0);
    delay(400); 
    float humidity = sensor.getRH();
    float tempC = sensor.getTemp();
    delay(400);

    Serial.print("  Temp(c):");
    Serial.print(Tb_i);
    Serial.print("  Pressure(hPa):");
    Serial.print(P_i);
    Serial.print("  Altitude(m):");
    Serial.print(a_i);  
    Serial.print("  Temp(c):");
    Serial.print(Th_i);
    Serial.print("  Humidity (%):");
    Serial.print(H_i); 
    Serial.println();  
      
    Tb_i = (int) (T*100);
    packet[0] = Tb_i >> 8;
    packet[1] = Tb_i;

    P_i = (int) (P*100);
    packet[2] = P_i >> 16;
    packet[3] = P_i >> 8;
    packet[4] = P_i;


    a_i = (int) (a*100);
    packet[5] = a_i >> 16;
    packet[6] = a_i >> 8;
    packet[7] = a_i;
    
    Th_i = (int) (tempC*100);
    packet[8] = Th_i >> 8;
    packet[9] = Th_i;

    H_i = (int) (humidity*100);
    packet[10] = H_i >> 8;
    packet[11] = H_i;
}
