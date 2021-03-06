/**
   @brief Simple ATMS Library
*/

/*Author: Gustavo Diaz*/

/*Requiered Libraries*/
#include <Arduino.h>
#include <SparkFunMPU9250-DMP.h>
#include "imu_data.h"
#include "logger.h"

/**
   @class IMU
   @brief Class for manage IMU
*/

class IMU9250
{
    /*Private Members*/
    MPU9250_DMP imu_;

  public:
    /*Public Members*/
    // orientation/motion vars
    Quaternion q;           // [w, x, y, z]         quaternion container
    VectorInt16 ws;         // [x, y, z]            gyro angular rates
    VectorInt16 aa;         // [x, y, z]            accel sensor measurements
    VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
    VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
    VectorFloat gravity;    // [x, y, z]            gravity vector
    VectorInt16 gyroRate;   // [x, y, z]            gyro rate
    float euler[3];         // [psi, theta, phi]    Euler angle container
    float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
    int16_t ax_raw, ay_raw, az_raw;
    int16_t gx_raw, gy_raw, gz_raw;
    ImuData imuData;

    /*constructor de base (null)*/
    IMU9250() {}

    // methods
    void init(void);
    void updateData(void);
    void infoPrint(void);

    // private:
    // ...methods
};


//------------------------------------------------------------------- Methods Definitions --------------------------------------------------------------
// TODO: cpp file

//-------------------------- Public Methods --------------------------
void dmpDataReady()
{
    mpuInterrupt = true;
}
void IMU::init(void)
{
    // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
        // Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif

    /*initialize imu*/
    DEBUG2_PRINTLN(F("Initializing I2C devices..."));
    mpu.initialize();
    pinMode(interrupt_pin_, INPUT);

    // verify connection
    DEBUG2_PRINTLN(F("Testing device connections..."));
    DEBUG2_PRINTLN(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

    // load and configure the DMP
    DEBUG2_PRINTLN(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();

    // supply your own gyro offsets here, scaled for min sensitivity
    mpu.setXGyroOffset(220);
    mpu.setYGyroOffset(76);
    mpu.setZGyroOffset(-85);
    mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

     // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        DEBUG2_PRINTLN(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        // // enable Arduino interrupt detection
        DEBUG2_PRINTLN(F("Enabling interrupt detection (Arduino external interrupt 0)..."));
        attachInterrupt(digitalPinToInterrupt(interrupt_pin_), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        DEBUG2_PRINTLN(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        DEBUG2_PRINTLN(F("DMP Initialization failed (code "));
        DEBUG2_PRINTLN(devStatus);
        DEBUG2_PRINTLN(F(")"));
    }
    DEBUG2_PRINT(F("packetSize:"));DEBUG2_PRINTLN(packetSize);
    DEBUG2_PRINT(F("fifoCount:"));DEBUG2_PRINTLN(fifoCount);
}

void IMU::updateData(void)
{
    if (!mpuInterrupt && fifoCount < packetSize)
    // if (!mpuInterrupt)
    // if (fifoCount < packetSize)
    {
        DEBUG2_PRINTLN(F("fifoCount < packetSize"));
        return;
    }
    else
    {
        // reset interrupt flag and get INT_STATUS byte
        mpuInterrupt = false;
        mpuIntStatus = mpu.getIntStatus();

        // get current FIFO count
        fifoCount = mpu.getFIFOCount();

        // check for overflow (this should never happen unless our code is too inefficient)
        if ((mpuIntStatus & 0x10) || fifoCount == 1024)
        // if ((mpuIntStatus & 0x10))
        {
            // reset so we can continue cleanly
            mpu.resetFIFO();
            DEBUG2_PRINTLN(F("FIFO overflow!"));

        // otherwise, check for DMP data ready interrupt (this should happen frequently)
        }
        else if (mpuIntStatus & 0x02)
        {
            DEBUG2_PRINTLN(F("reading"));
            // wait for correct available data length, should be a VERY short wait
            while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

            // read a packet from FIFO
            mpu.getFIFOBytes(fifoBuffer, packetSize);
            
            // track FIFO count here in case there is > 1 packet available
            // (this lets us immediately read more without waiting for an interrupt)
            fifoCount -= packetSize;

            #ifdef OUTPUT_READABLE_QUATERNION
                mpu.dmpGetQuaternion(&q, fifoBuffer);
            #endif

            #ifdef OUTPUT_READABLE_YAWPITCHROLL
                // display ypr angles in degrees
                mpu.dmpGetQuaternion(&q, fifoBuffer);
                mpu.dmpGetGravity(&gravity, &q);
                mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
            #endif

            #ifdef OUTPUT_READABLE_REALACCEL
                // display real acceleration, adjusted to remove gravity
                mpu.dmpGetQuaternion(&q, fifoBuffer);
                mpu.dmpGetAccel(&aa, fifoBuffer);
                mpu.dmpGetGravity(&gravity, &q);
                mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
            #endif

            #ifdef OUTPUT_READABLE_WORLDACCEL
                // display initial world-frame acceleration, adjusted to remove gravity
                // and rotated based on known orientation from quaternion
                mpu.dmpGetQuaternion(&q, fifoBuffer);
                mpu.dmpGetAccel(&aa, fifoBuffer);
                mpu.dmpGetGravity(&gravity, &q);
                mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
                mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);
            #endif
            mpu.dmpGetGyro(&ws, fifoBuffer);
            mpu.getMotion6(&ax_raw, &ay_raw, &az_raw, &gx_raw, &gy_raw, &gz_raw);
        }
        else{
            // DEBUG2_PRINTLN(F("mpuIntStatus"));
        }
        mpu.dmpGetGyro(&gyroRate, fifoBuffer);
    }
}
