/**
 * @file   accelerometer.c
 * @brief  Accelerometer Adafruit BNO55
 * @date   2019-03-13
 * @author Oleksandra Baga
 */

/******************************************************************************/
/* Accelerometer Adafruit BNO55                             */
/******************************************************************************/
/* This file is part of the project for university course   */
/* Advanced ARM Programming                                 */
/* Wintersemester 2018/2019                                 */
/******************************************************************************/
#include "accelerometer.h"

#pragma anon_unions  /* Enable and disable support for anonymous structures and unions */

#include <LPC23xx.H> /* LPC23xx definitions             */
#include <string.h>
#include <time.h>
#include "include/ada_sensor.h"


#define l_delay 5000000
#define s_delay 500000

/* Global variables */
unsigned char I2Cdata; ///< Data to send into I2C
int16_t I2Cmessage;    ///< Received data from I2C
unsigned int BNOI2CAddress = 0x50;  ///< 01010000 Device (Slave) Address (7 bits) shifted BNO055_I2C_ADDR1 (0x28)

/*
 * With the ALT ADDRESS pin high, the 7-bit I2C address for the device is 0x1D, followed by the R/W bit.
 * This translates to 0x3A for a write and 0x3B for a read.
 * But 0x3A is used for read and write, the i2c_irq will add read bit during read transfer
 */
unsigned int ADXLI2CAdresss = 0x3A;

unsigned char GlobalI2CAddr;
unsigned char GlobalI2CReg;
unsigned char GlobalI2CData;
unsigned char GlobalI2CRead;

volatile enum {
    I2C_IDLE,
    I2C_ADR,
    I2C_STARTED,
    I2C_RESTARTED,
    I2C_REG,
    I2C_DAT,
    I2C_DAT_ACK,
    I2C_RESTART,
    I2C_DAT_NACK,
    I2C_WRITE,
    I2C_READ,
    I2C_ERR,
    I2C_LOST,
    I2C_DONE
} GlobalI2CState;

volatile int ReadLenght = 1;
unsigned int ReadIndex = 0;
volatile int WriteLenght = 1;
unsigned int WriteIndex = 0;
unsigned char I2CReadBuffer[2];
unsigned char I2CMasterBuffer[2]; ///< For multibyte read: keeps 2 registers
unsigned char XRegs[] = {ADXL345_REG_DATAX0, ADXL345_REG_DATAX1};
unsigned char YRegs[] = {ADXL345_REG_DATAY0, ADXL345_REG_DATAY1};
unsigned char ZRegs[] = {ADXL345_REG_DATAZ0, ADXL345_REG_DATAZ1};
volatile uint8_t DebugI2CState;

/*!
 *   @brief  Hard-coded _delay
 */
void _delay(int _delay) {
    int i;
    for (i = 0; i < _delay; ++i) {

    };
}

/*!
 * @brief LCD design for printing initial greeting
 */
void lcd_print_greeting(void) {
    lcd_clear();
    lcd_print ("Water Cube");
    set_cursor (0, 1);
    lcd_print ("Beuth Hochschule");
    set_cursor (0, 0);
}
/**
 * @brief LCD design for printing any message
 */
void lcd_print_message(unsigned char *msg) {
    lcd_clear();
    set_cursor (0, 0);
    lcd_print(msg);
}
/**
 * @brief LCD design for printing coordinates x,y,z from accelerometer
 */
void lcd_print_coordinates(unsigned char *msg) {
    lcd_clear();
    set_cursor (0, 0);
    lcd_print ("X:    Y:     Z:");
    set_cursor (0, 1);
    lcd_print(msg);
}

/*!
 * @brief  I2C Interrupt Service Routine
 *         State machine examines the status register on each interrupt and
 *         performs the necessary action
 */
__irq void i2c_irq(void) {
    DebugI2CState = I21STAT;

    switch (I21STAT) // Read result code and switch to next action
    {
    case (0x08): // A START condition has been transmitted. Next: 0x18
        // Case ADR -> STARTED
        I21CONCLR = 0x20; // Clear start bit
        I21DAT = GlobalI2CAddr; // Send address and write bit R/W = 0.
        I21CONSET = 0x04; // Write 0x04 to I2CONSET to set the AA bit
        I21CONCLR = 0x08; // Clear SI flag
        GlobalI2CState = I2C_STARTED;
        break;

    case (0x10): // A repeated START condition has been transmitted. Next: 0x18
        if(GlobalI2CState == I2C_READ) {
            // Write Slave Address with R/W bit to I2DAT
            I21DAT = GlobalI2CAddr | 1; // Send address and read bit R/W = 1
        }
        if(GlobalI2CState == I2C_WRITE) {
            // Write Slave Address with R/W bit to I2DAT
            I21DAT = GlobalI2CAddr; // Send address and read bit R/W = 0
        }
        I21CONSET = 0x04; // Write 0x04 to I2CONSET to set the AA bit
        I21CONCLR = 0x28; // Clear start bit and SI flag
        break;

    // Previous state was State 8 or State 10, Slave Address + Write has been transmitted, ACK has been received.
    // The first data byte will be transmitted, an ACK bit will be received.
    case (0x18): // Next: 0x28
        // Case STARTED -> REG
        //I21DAT = GlobalI2CReg; // Write data to TX register
        I21DAT = I2CMasterBuffer[ReadIndex]; // Write data to TX register
        //ReadIndex++;
        I21CONCLR = 0x08; // Clear SI bit
        GlobalI2CState = I2C_REG; // with re-start for write
    break;

    case (0x20): // SLA+W has been transmitted; NOT ACK has been received
        I21DAT = GlobalI2CData;
        I21CONCLR = 0x08; // clear SI flag
        GlobalI2CState = I2C_DAT;
        break;

    case (0x28): // Data byte in I2DAT has been transmitted; ACK has been received.
    case (0x30): // transmitted, NOT ACK has been received
        // Repeated Start for Read Operation
        // Case RESTART -> READ
        if (GlobalI2CState == I2C_REG && GlobalI2CRead) {
            I21CONSET = 0x24; // Repeated start condition for Read Access
            I21CONCLR = 0x08; // clear SI flag
            GlobalI2CState = I2C_READ;
        }
        // Case RESTART -> WRITE
        else if (GlobalI2CState == I2C_REG && !GlobalI2CRead) {
            // with re-start for write
            //I21CONSET = 0x24; // Repeated start condition for Write Access
            //I21CONCLR = 0x08; // clear SI flag
            GlobalI2CState = I2C_WRITE;
        }

        if (GlobalI2CState == I2C_WRITE) {
            I21DAT = GlobalI2CData;
            GlobalI2CState = I2C_DAT;
            I21CONCLR = 0x08; // clear SI flag
        }
        else if (GlobalI2CState == I2C_DAT) {
            GlobalI2CState = I2C_DONE;
            I21CONSET = 0x10; // STO
            I21CONCLR = 0x08; // clear SI flag
        }

        break;

    case (0x38): // Arbitration lost
        I21CONSET = 0x24; // to set the STA and AA bits
        I21CONCLR = 0x08; // clear SI flag
        GlobalI2CState = I2C_LOST;
        break;

    case (0x40) : // SLA+R has been transmitted; ACK has been received. Next: 0x50
        I21CONSET = 0x04; // Enable ACK for data byte
        I21CONCLR = 0x08; // clear SI flag
        break;

    case (0x48) : // Slave Address +R, Not Ack
        I21CONSET = 0x14; // set the STO and AA bits
        I21CONCLR = 0x08; // clear SI flag
        GlobalI2CState = I2C_ERR;
        break;

    /* Data has been received, ACK has been returned. Data will be read from I2DAT.
    Additional data will be received. If this is the last data byte then NOT ACK will be returned,
    otherwise ACK will be returned */
    case (0x50):
    case (0x58): // Data Received, Not Ack
        if (ReadIndex != ReadLenght) {
            GlobalI2CData = I21DAT;
            I2CReadBuffer[ReadIndex] = I21DAT;
            ReadIndex++;
            I21CONCLR = 0x0C; //clear the SI flag and the AA bit
            GlobalI2CState = I2C_DAT_ACK;
        }
        else {
            ReadIndex = 0;
            I21CONSET = 0x10; // set STO
            GlobalI2CState = I2C_DONE;
        }
        I21CONCLR = 0x08; // clear SI flag
        break;

    default:
        break;
    }

    VICVectAddr = 0x00000000; // Clear interrupt in
}

/*!
 * @brief  Writes 8-bits to the specified destination register
 */
void I2CWriteReg(unsigned char addr, unsigned char reg, unsigned char data) {
    ReadLenght = 1;
    ReadIndex = 0;
    GlobalI2CAddr = addr;
    GlobalI2CReg = reg;
    I2CMasterBuffer[0] = reg;
    GlobalI2CData = data;
    GlobalI2CRead = 0;
    GlobalI2CState = I2C_ADR;

    I21CONSET = 0x20; // Start condition

    while((GlobalI2CState != I2C_ERR) && (GlobalI2CState != I2C_DONE)) {
        ;
    }
    // Debug print
    //lcd_print_message("Write exit");
    ReadLenght = 1;
    GlobalI2CState = I2C_IDLE;
}

/*!
 * @brief  Reads 8-bits from the specified register
 */
uint8_t I2CReadReg(unsigned char addr, unsigned char reg) {
    ReadLenght = 1;
    ReadIndex = 0;
    GlobalI2CAddr = addr;
    //GlobalI2CReg = reg;
    I2CMasterBuffer[0] = reg;
    GlobalI2CRead = 1;
    GlobalI2CState = I2C_ADR;

    I21CONSET = 0x20; // Start condition

    while((GlobalI2CState != I2C_ERR) && (GlobalI2CState != I2C_DONE)) {
        ;
    }

    GlobalI2CState = I2C_IDLE;
    ReadLenght = 1;
    ReadIndex = 0;

    return GlobalI2CData;
}

/*!
 * @brief  Reads 16-bits from the specified register
 */
uint16_t I2CRead16Bits(unsigned char addr, unsigned char regs[]) {
    uint16_t i2c16bit = 0x00;
    ReadLenght = 2;
    GlobalI2CAddr = addr;
    I2CMasterBuffer[0] = regs[0];
    I2CMasterBuffer[1] = regs[1];
    GlobalI2CRead = 1;
    GlobalI2CState = I2C_ADR;

    I21CONSET = 0x20; // Start condition

    while((GlobalI2CState != I2C_ERR) && (GlobalI2CState != I2C_DONE)) {
        ;
    }

    GlobalI2CState = I2C_IDLE;
    ReadLenght = 1;
    // merge the data from two registers
    i2c16bit = i2c16bit | I2CReadBuffer[1]; // [REG0, REG1]: REG1 as MSB
    i2c16bit = i2c16bit << 8;

    i2c16bit = i2c16bit | I2CReadBuffer[0]; // [REG0, REG1]: REG0 as LSB

    return i2c16bit;
}

/*!
 * @brief  Gets the most recent X axis value
 */
int16_t getX(void) {
    return I2CRead16Bits(ADXLI2CAdresss, XRegs);
}

/*!
 * @brief  Gets the most recent Y axis value
 */
int16_t getY(void) {
    return I2CRead16Bits(ADXLI2CAdresss, YRegs);
}

/*!
 * @brief  Gets the most recent Z axis value
 */
int16_t getZ(void) {
    return I2CRead16Bits(ADXLI2CAdresss, ZRegs);
}

/*!
 * @brief  I2C init
 * 1. configure the VIC to respond to a I2C interrupt
 * 2. configure pinselect block to connect the I2C data and clock lines to the external pins
 * 3. set the bit rate by programming I2SCLH and I2SCLL.
 *      Only the first 16 bits are used to hold the timing values
*/
void i2c_init(void) {
    PCONP |= 0x00080000;
    _delay(s_delay);

    VICVectCntl19 = 0x0000001;       // select a priority slot for a given interrupt
    VICVectAddr19 = (unsigned)i2c_irq; //pass the address of the IRQ into the VIC slot
    VICIntEnable |= 0x00080000; // enable interrupt
    PINSEL1 |= 0x000003C0; //Switch GPIO to I2C pins
    /* Set bit rate
    Fcco = 2 * M * Fin / N = 2 * (MSEL +1 ) * 12 MHz / 1 = 2 * 12 * 12 * 10^6 = 288 * 10^6
    Devider CCLKCFG_Val = 0x00000005 ==> 288 /6 = 48 * 10^6
    CCLKCFG_Val =  0x00000000 ===> 48 / 4 = 12 * 10^6 == Input Frequency
    BNO055 max frequency 400 KHz = 4 * 10^5
    SCLH + SCLL = 60 */
    // 400 KHz
    I21SCLH = 0xF;
    I21SCLL  = 0xF;
    // 200 KHz
    //I21SCLL  = 30;
    //I21SCLH  = 30;

    I21CONCLR = 0x000000FF; // Clear all I2C settings
    _delay(s_delay);
    // Before the master transmitter mode can be entered, I2CONSET must be initialized with 0100 0000
    I21CONSET = 0x00000040; // Enable the I2C interface

    GlobalI2CState = I2C_IDLE;
}

/*!
 * @brief  ADXL345 Init
 * Note: I2C should be already enabled
 */
int accelerometer_init() {
    volatile uint8_t id;

    id = I2CReadReg(ADXLI2CAdresss, ADXL345_REG_DEVID);

    if (id != ADXL345_ID) {
        return 0;
    }

    //Enable Measurements
    I2CWriteReg(ADXLI2CAdresss, ADXL345_REG_POWER_CTL, MEASURE);
    // set the interrupts to active low.
    I2CWriteReg(ADXLI2CAdresss, ADXL345_REG_DATA_FORMAT, INT_INVERT);
    // set the interrupts to active low.
    I2CWriteReg(ADXLI2CAdresss, ADXL345_REG_DATA_FORMAT, ADXL345_RANGE_4_G);
    //The DATA_READY bit is set when new data is available and is cleared when no new data is available.
    I2CWriteReg(ADXLI2CAdresss, ADXL345_REG_INT_ENABLE, DATA_READY);
    //Sent the Data Ready Interrupt to the INT1 pin
    I2CWriteReg(ADXLI2CAdresss, ADXL345_REG_INT_MAP, 0<<7);
    //Set Output Rate to 400 Hz
    I2CWriteReg(ADXLI2CAdresss, ADXL345_REG_BW_RATE, ADXL345_DATARATE_400_HZ);

    return 1;
}
