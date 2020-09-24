//
//   SDL_ESP32_WeatherRack2 Library
//   SDL_ESP32_WeatherRack2.cpp
//   Version 1.2
//   SwitchDoc Labs   September 2020
//
//
/*
  SDL_ESP32_WeatherRack2
  September 2020
  Added ESP32 Support

  Modified by SwitchDoc Labs to support WeatherRack2 and SDL Indoor TH sensor.


  Based on Andrew's sketch for capturing data from F007th Thermo-Hygrometer and with the addition of a checksum check.


  Inspired by the many weather station hackers who have gone before,
  Only possible thanks to the invaluable help and support on the Arduino forums.

  With particular thanks to

  Rob Ward (whose Manchester Encoding reading by delay rather than interrupt
  is the basis of this code)
  https://github.com/robwlakes/ArduinoWeatherOS

  The work of 3zero8 capturing and analysing the F007th data
  http://forum.arduino.cc/index.php?topic=214436.0

  The work of Volgy capturing and analysing the F007th data
  https://github.com/volgy/gr-ambient

  Marco Schwartz for showing how to send sensor data to websites
  http://www.openhomeautomation.net/

  The forum contributions of;
  dc42: showing how to create 5 minute samples.
  jremington: suggesting how to construct error checking using averages (although not used this is where the idea of using newtemp and newhum for error checking originated)
  Krodal: for his 6 lines of code in the forum thread "Compare two sensor values"

  This example code is in the public domain.

  Additional work by Ron Lewis on reverse engineering the checksum

  What this code does:
  Captures F007th (and now TF0300) Thermo-Hygrometer data packets by;
  Identifying a header of at least 10 rising edges (manchester encoding binary 1s)
  Synchs the data correctly within byte boundaries
  Distinguishes between F007th data packets and other 434Mhz signals with equivalent header by checking value of sensor ID byte
  Correctly identifies positive and negative temperature values to 1 decimal place for up to 8 channels
  Correctly identifies humidity values for up to 8 channels
  Error checks data by rejecting;
  humidity value outside the range 1 to 100%
  bad checksums


  Code optimisation



  F007th Thermo-Hygrometer
  Sample Data:
  0        1        2        3        4        5        6        7
  FD       45       4F       04       4B       0B       52       0
  0   1    2   3    4   5    6   7    8   9    A   B    C   D    E
  11111101 01000101 01001111 00000100 01001011 00001011 01010010 0000
  hhhhhhhh SSSSSSSS NRRRRRRR bCCCTTTT TTTTTTTT HHHHHHHH CCCCCCCC ????

  Channel 1 F007th sensor displaying 21.1 Centigrade and 11% RH

  hhhhhhhh = header with final 01 before data packet starts (note using this sketch the header 01 is omitted when the binary is displayed)
  SSSSSSSS = sensor ID, F007th = Ox45
  NRRRRRRR = Rolling Code Byte? Resets each time the battery is changed
  b = battery indicator?
  CCC = Channel identifier, channels 1 to 8 can be selected on the F007th unit using dipswitches. Channel 1 => 000, Channel 2 => 001, Channel 3 => 010 etc.
  TTTT TTTTTTTT = 12 bit temperature data.
  To obtain F: convert binary to decimal, take away 400 and divide by 10 e.g. (using example above) 010001001011 => 1099
  (1099-400)/10= 69.9F
  To obtain C: convert binary to decimal, take away 720 and multiply by 0.0556 e.g.
  0.0556*(1099-720)= 21.1C
  HHHHHHHH = 8 bit humidity in binary. e.g. (using example above) 00001011 => 11
  CCCCCCCC = checksum? Note that this sketch only looks at the first 6 bytes and ignores the checksum

  The F0300 sends about 9 additional bytes of data for the WeatherRack2 and is formatted differently

*/

#undef WR2DEBUG

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif


#include "SDL_ESP32_WeatherRack2.h"

// pins
int RxPin           = 32;   //The number of signal from the Rx

#ifdef WR2DEBUG
int PinTest = 15;
int pinValue  = 0;
int PinTestF0300 = 27;
int pinValueF0300  = 0;

int messageFoundPin = 14;
int PinHeaderTest = 33;
int pinHeaderValue = 0;
int PinHeaderBitTest = 12;
int pinHeaderBitValue = 0;
#endif

// Class Functions

SDL_ESP32_WeatherRack2::SDL_ESP32_WeatherRack2(long timeout, boolean read_weatherrack2 , boolean read_indoorth   )
{
  _timeout = timeout;
  _read_weatherrack2 = read_weatherrack2;
  _read_indoorth = read_indoorth;
}

void SDL_ESP32_WeatherRack2::begin()
{

  headersFound = 0;
  FT300MessagesFound = 0;
  FT007MessagesFound = 0;
  ErrorJSON = "{\"Type\" : \"None\"}";
  TimeOutJSON = "{\"Type\" : \"TimeOut\"}";

  currentJSON = ErrorJSON;
  messageID = 0;

  pinMode(RxPin, INPUT);
#ifdef WR2DEBUG
  pinMode(PinTest, OUTPUT);
  pinMode(PinHeaderTest, OUTPUT);
  pinMode(PinHeaderBitTest, OUTPUT);
  pinMode(PinTestF0300, OUTPUT);
  pinMode(messageFoundPin, OUTPUT);
  digitalWrite(PinTest, LOW);
  digitalWrite(PinTestF0300, LOW);
  digitalWrite(messageFoundPin, LOW);
  digitalWrite(PinHeaderTest, LOW);
  digitalWrite(PinHeaderBitTest, LOW);
#endif

  eraseManchester();  //clear the array to different nos cause if all zeroes it might think that is a valid 3 packets ie all equal

}


String SDL_ESP32_WeatherRack2::getCurrentJSON()
{
  return currentJSON;
}

String SDL_ESP32_WeatherRack2::waitForNextJSON()
{


  currentJSON = returnMessageJSON();


}

void SDL_ESP32_WeatherRack2::setTimeout(long my_timeout)
{
  _timeout = my_timeout;
}

void SDL_ESP32_WeatherRack2::set_ReadWeatherRack2(boolean my_read_weatherrack2)
{
  _read_weatherrack2 = my_read_weatherrack2;

}

void SDL_ESP32_WeatherRack2::set_ReadIndoorth(boolean my_readindoorth)
{

  _read_indoorth = my_readindoorth;
}

long SDL_ESP32_WeatherRack2::readHeadersFound()
{

  return headersFound;
}

long SDL_ESP32_WeatherRack2::readWeatherRack2Found()
{
  return FT300MessagesFound;

}

long SDL_ESP32_WeatherRack2::readSDLIndoorTHFound()
{

  return FT007MessagesFound;

}


//Internal functions


// Libraries
#include <SPI.h>

#define MAX_BYTES 7
#define countof(x) (sizeof(x)/sizeof(x[0]))
// Interface Definitions


// Variables for Manchester Receiver Logic:
word    sDelay     = 245;  //Small Delay about 1/4 of bit duration
word    lDelay     = 490;  //Long Delay about 1/2 of bit duration, 1/4 + 1/2 = 3/4
byte    polarity   = 1;    //0 for lo->hi==1 or 1 for hi->lo==1 for Polarity, sets tempBit at start
byte    tempBit    = 1;    //Reflects the required transition polarity
boolean firstZero  = false;//flags when the first '0' is found.
boolean noErrors   = true; //flags if signal does not follow Manchester conventions
//variables for Header detection
byte    headerBits = 9;   //The number of ones expected to make a valid header
byte    headerHits = 0;    //Counts the number of "1"s to determine a header
//Variables for Byte storage
boolean sync0In = true;    //Expecting sync0 to be inside byte boundaries, set to false for sync0 outside bytes
byte    dataByte   = 0xFF; //Accumulates the bit information
byte    nosBits    = 6;    //Counts to 8 bits within a dataByte
byte    maxBytes   = MAX_BYTES;    //Set the bytes collected after each header. NB if set too high, any end noise will cause an error
byte    nosBytes   = 0;    //Counter stays within 0 -> maxBytes
//Variables for multiple packets
byte    bank       = 0;    //Points to the array of 0 to 3 banks of results from up to 4 last data downloads
byte    nosRepeats = 3;    //Number of times the header/data is fetched at least once or up to 4 times
//Banks for multiple packets if required (at least one will be needed)
byte  manchester[20];   //Stores banks of manchester pattern decoded on the fly

// Variables to prepare recorded values for Ambient

byte stnId = 0;      //Identifies the channel number
int dataType = 0;    //Identifies the Ambient Thermo-Hygrometer code
int Newtemp = 0;
int Newhum = 0;
int ChanTemp[9];    //make one extra so we can index 1 relative
int ChanHum[9];


#ifdef WR2DEBUG

// Main RF, to find header, then sync in with it and get a packet.

void flipF0300Bit(int value)
{
  //pinValueF0300 = pinValueF0300 ^ 1;
  digitalWrite(PinTestF0300, value);

}


void  flipTestBit()
{
  pinValue = pinValue ^ 1;
  digitalWrite(PinTest, pinValue);

}

void sendMessageFound()
{
  digitalWrite(messageFoundPin, HIGH);
  delayMicroseconds(300);
  digitalWrite(messageFoundPin, LOW);
  delayMicroseconds(20);
}


void sendHeaderBitFound()
{

  pinHeaderBitValue = pinHeaderBitValue ^ 1;
  digitalWrite(PinHeaderBitTest, pinHeaderBitValue);
}


void sendHeaderFound()
{

  pinHeaderValue = pinHeaderValue ^ 1;
  digitalWrite(PinHeaderTest, pinHeaderValue);
}

#endif

long microLengthofPulse = 0;
boolean dataTypeDetected;




String SDL_ESP32_WeatherRack2::returnMessageJSON()
{

  String returnJSON;
  returnJSON = "";
  boolean messageFound = false;



  long endTime =   millis() + _timeout * 1000;


  while ((returnJSON.length() == 0) && (endTime > millis()))
  {
    tempBit = polarity; //these begin the same for a packet
    noErrors = true;
    firstZero = false;
    headerHits = 0;
    nosBits = 6;
    nosBytes = 0;
    long currentDelay = 0;
    dataTypeDetected = false;


    while (noErrors && (nosBytes < maxBytes))
    {
      microLengthofPulse = micros();
      boolean clearOnce = true;
      boolean NotFoundTran = true;
      while (NotFoundTran)
      {

        if (digitalRead(RxPin) != tempBit)
          NotFoundTran = true;
        else
          NotFoundTran = false;
        //pause here until a transition is found
        currentDelay = (micros() - microLengthofPulse);







      }//at Data transition, half way through bit pattern, this should be where RxPin==tempBit




      delayMicroseconds(sDelay);//skip ahead to 3/4 of the bit pattern
#ifdef WR2DEBUG
      flipTestBit();
#endif

      // 3/4 the way through, if RxPin has changed it is definitely an error
      if (digitalRead(RxPin) != tempBit)
      {
        noErrors = false; //something has gone wrong, polarity has changed too early, ie always an error
      }//exit and retry
      else
      {

        delayMicroseconds(lDelay);
#ifdef WR2DEBUG
        flipTestBit();
#endif
        //now 1 quarter into the next bit pattern,
        if (digitalRead(RxPin) == tempBit) //if RxPin has not swapped, then bitWaveform is swapping
        {
          //If the header is done, then it means data change is occuring ie 1->0, or 0->1
          //data transition detection must swap, so it loops for the opposite transitions
          tempBit = tempBit ^ 1;
        }//end of detecting no transition at end of bit waveform, ie end of previous bit waveform same as start of next bitwaveform

        //Now process the tempBit state and make data definite 0 or 1's, allow possibility of Pos or Neg Polarity
        byte bitState = tempBit ^ polarity;//if polarity=1, invert the tempBit or if polarity=0, leave it alone.


        if (bitState == 1) //1 data could be header or packet
        {
          if (!firstZero)
          {
            headerHits++;
#ifdef WR2DEBUG
            sendHeaderBitFound();
#endif

            if (headerHits == headerBits)
            {
              //Serial.print("H");
#ifdef WR2DEBUG
              sendHeaderFound();
#endif
              headersFound++;
            }

          }
          else
          {
            returnJSON = add(bitState);//already seen first zero so add bit in
          }
        }//end of dealing with ones
        else
        { //bitState==0 could first error, first zero or packet
          // if it is header there must be no "zeroes" or errors
          if (headerHits < headerBits)
          {
            //Still in header checking phase, more header hits required
            noErrors = false; //landing here means header is corrupted, so it is probably an error
          }//end of detecting a "zero" inside a header
          else
          {
            //we have our header, chewed up any excess and here is a zero
            if (!firstZero) //if first zero, it has not been found previously
            {
              firstZero = true;
              returnJSON = add(bitState);//Add first zero to bytes
              //Serial.print("!");
            }//end of finding first zero
            else
            {
              returnJSON = add(bitState);
            }//end of adding a zero bit
          }//end of dealing with a first zero
        }//end of dealing with zero's (in header, first or later zeroes)
      }//end of first error check

    }//end of while noErrors=true and getting packet of bytes

  } //end of while


  if (endTime <= millis())
    returnJSON = TimeOutJSON;

  if (returnJSON.length() == 0)
    returnJSON = ErrorJSON;
  return returnJSON;
}

//Read the binary data from the bank and apply conversions where necessary to scale and format data

String SDL_ESP32_WeatherRack2::add(byte bitData)
{
#ifdef WR2DEBUG
  flipF0300Bit(bitData);
#endif
  dataByte = (dataByte << 1) | bitData;
  nosBits++;
  if (nosBits == 8)
  {
    nosBits = 0;
    manchester[nosBytes] = dataByte;
    nosBytes++;
    //Serial.print("B");
    //Serial.print(" ");
    //Serial.print(nosBytes);
    //Serial.print(dataByte, HEX);
    //Serial.print(" ");
  }


  if ((maxBytes != 16) && (nosBytes > 1) && (!dataTypeDetected))
  {

    dataType = manchester[1];

    if (dataType == 0x4C) {

#ifdef WR2DEBUG
      Serial.println (dataType, HEX);
#endif
      maxBytes = 16;
      dataTypeDetected = true;
    }




  }

  if (nosBytes == maxBytes)
  {


    dataByte = 0xFF;
    // Subroutines to extract data from Manchester encoding and error checking

    // Identify channels 1 to 8 by looking at 3 bits in byte 3
    int stnId = ((manchester[3] & B01110000) / 16) + 1;

    int battery = (manchester[3] & B1000000);
    String myBattery;
    if (battery == 0)
      myBattery = "OK";
    else
      myBattery = "LOW";

    float tempc;


    // Identify sensor by looking for sensorID in byte 1 (F007th  Thermo-Hygrometer = 0x45)
    dataType = manchester[1];
    int device = manchester[2];

    // Gets raw temperature from bytes 3 and 4 (note this is neither C or F but a value from the sensor)
    Newtemp = (float((manchester[3] & B00000111) * 256) + manchester[4]);

    // Gets humidity data from byte 5
    Newhum = (manchester [5]);

    int myFT007CalculatedChecksum = Checksum(7 - 2, manchester + 1);
    int myFT007Checksum = manchester[6];
#ifdef WR2DEBUG
    Serial.print("007CalChecksum=");
    Serial.println(myFT007CalculatedChecksum, HEX);
    Serial.print("myFT007Checksum=");
    Serial.println(myFT007Checksum, HEX);
#endif


    if (myFT007CalculatedChecksum == myFT007Checksum)
    {


      // Checks sensor is a F007th with a valid humidity reading equal or less than 100
      if ((dataType == 0x45) && (Newhum <= 100))
      {
        FT007MessagesFound++;
        //sendMessageFound();

        ChanTemp[stnId] = Newtemp;
        ChanHum[stnId] = Newhum;

        // print raw data
#ifdef WR2DEBUG
        char Buff[128];

        for (int i = 0; i < maxBytes; i++)
        {
          sprintf(&Buff[i * 3], "%02X ", manchester[i]);
        }

        // 123456789 123456789 1234
        // Channel=1 F=123.1 H=12%<
        sprintf(&Buff[maxBytes * 3], "  Channel=%d F=%d.%d H=%d%%\n",
                stnId,
                (Newtemp - 400) / 10,
                Newtemp - (Newtemp / 10) * 10,
                Newhum);
        Serial.print(Buff);



        Serial.print("Headers found ="); Serial.println(headersFound);
        Serial.print("FT007 Messages found="); Serial.println(FT007MessagesFound);
        Serial.print("FT300 Messages found="); Serial.println(FT300MessagesFound);
#endif
        tempc = float(Newtemp - 400) / 10.0;
        tempc = (tempc - 32.0) * (5.0 / 9.0);

      }


      messageID++;

      String myJSON;
      myJSON =  "{\"messageid\" : \"" + String(messageID) + "\", ";
      myJSON +=  "\"time\" : \"\", ";
      myJSON +=  "\"model\" : \"SwitchDoc Labs F007TH Thermo-Hygrometer\", ";
      myJSON +=  "\"device\" : \"" + String(device) + "\", ";
      myJSON +=  "\"modelnumber\" : \"5\", ";
      myJSON +=  "\"channel\" : \"" + String(stnId) + "\", ";
      myJSON +=  "\"battery\" : \"" + myBattery + "\", ";
      myJSON +=  "\"temperature\" : \"" + String(tempc) + "\", ";
      myJSON +=  "\"humidity\" : \""     + String(Newhum) + "\", ";
      myJSON +=  "\"CRC\" : \"" + String(myFT007CalculatedChecksum, HEX) + "\"}";
      return myJSON;

    }


    if ((dataType == 0x4C))
    {
#ifdef WR2DEBUG
      Serial.println("FT0300 found");
#endif
      FT300MessagesFound++;
#ifdef WR2DEBUG
      sendMessageFound();
      sendMessageFound();
#endif
      // print raw data
      char Buff[128];

      uint8_t  myDevice;
      uint8_t  mySerial;
      uint8_t  myFlags;
      uint8_t myBatteryLow;
      uint16_t  myAveWindSpeed;
      uint16_t  myGust;
      uint16_t  myWindDirection;
      uint32_t myCumulativeRain;
      uint8_t mySecondFlags;
      uint16_t myTemperature;
      uint8_t  myHumidity;
      uint32_t myLight;
      uint16_t myUV;
      uint8_t myCRC;
      uint8_t b2[18];
      uint8_t myCalculated;


      int i;

      b2[0] = 0xd4; // Shift all 4 bits fix b2[0];
      for (i = 0; i < 17; i++)
      {
        b2[i + 1] = ((manchester[i + 1] & 0x0f) << 4) + ((manchester[i + 2] & 0xf0) >> 4);
        b2[i] = b2[i + 1]; // shift 8

      }

#ifdef WR2DEBUG
      for (i = 0; i < 17; i++)
      {
        Serial.print("b2[");
        Serial.print(i);
        Serial.print("]=");
        Serial.print( b2[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
#endif



      uint8_t expected = b2[13];

      myCalculated = GetCRC(0xc0, b2, 13);
      myCRC = b2[13];
      if (myCalculated == myCRC)
      {


        uint8_t calculated = myCalculated;

        myDevice = (b2[0] & 0xf0) >> 4;
        //if (myDevice !=  0x0c)
        //{
        //return 0; // not my device
        //}
        mySerial = (b2[0] & 0x0f) << 4 & (b2[1] % 0xf0) >> 4;
        myFlags  = b2[1] & 0x0f;
        myBatteryLow = (myFlags & 0x08) >> 3;
        myAveWindSpeed = b2[2] | ((myFlags & 0x01) << 8);
        myGust         = b2[3] | ((myFlags & 0x02) << 7);
        myWindDirection = b2[4] | ((myFlags & 0x04) << 6);
        myCumulativeRain = (b2[5] << 8) + b2[6];
        mySecondFlags  = (b2[7] & 0xf0) >> 4;
        myTemperature = ((b2[7] & 0x0f) << 8) + b2[8];
        myHumidity = b2[9];
        myLight = (b2[10] << 8) + b2[11] + ((mySecondFlags & 0x08) << 9);
        myUV = b2[12];
        //myUV = myUV + 10;

        String myBattery;
        if (myBatteryLow == 0)
          myBattery = "OK";
        else
          myBattery = "LOW";


#ifdef WR2DEBUG

        Serial.print("myDevice = "); Serial.print(myDevice, HEX  ); Serial.print(" "); Serial.println(myDevice);
        Serial.print("mySerial = "); Serial.print(mySerial, HEX  ); Serial.print(" "); Serial.println(mySerial );
        Serial.print("myFlags = "); Serial.print(myFlags, HEX  ); Serial.print(" "); Serial.println(myFlags );
        Serial.print("myBatteryLow = "); Serial.print( myBatteryLow, HEX   ); Serial.print(" "); Serial.println( myBatteryLow  );
        Serial.print("myAveWindSpeed = "); Serial.print( myAveWindSpeed, HEX  ); Serial.print(" "); Serial.println( myAveWindSpeed );
        Serial.print("myGust = "); Serial.print( myGust, HEX  ); Serial.print(" "); Serial.println( myGust );
        Serial.print("myWindDirection = "); Serial.print(  myWindDirection, HEX  ); Serial.print(" "); Serial.println(  myWindDirection  );
        Serial.print("myCumulativeRain = "); Serial.print(  myCumulativeRain, HEX  ); Serial.print(" "); Serial.println( myCumulativeRain  );
        Serial.print("mySecondFlags = "); Serial.print( mySecondFlags, HEX  ); Serial.print(" "); Serial.println( mySecondFlags );
        Serial.print("myTemperature = "); Serial.print( myTemperature, HEX  ); Serial.print(" "); Serial.println( myTemperature);
        Serial.print("myHumidity = "); Serial.print( myHumidity, HEX  ); Serial.print(" "); Serial.println( myHumidity);
        Serial.print("myLight = "); Serial.print( myLight, HEX  ); Serial.print(" "); Serial.println( myLight );
        Serial.print("myUV = "); Serial.print( myUV, HEX  ); Serial.print(" "); Serial.println( myUV );
        Serial.print("myCRC = "); Serial.print(  myCRC, HEX  ); Serial.print(" "); Serial.println(  myCRC);
        Serial.print("myCalculated  ="); Serial.print(  myCalculated, HEX  ); Serial.print(" "); Serial.println(  myCalculated  );

        Serial.print("Headers found ="); Serial.println(headersFound);
        Serial.print("FT007 Messages found="); Serial.println(FT007MessagesFound);
        Serial.print("FT300 Messages found="); Serial.println(FT300MessagesFound);
#endif
        float tempc;

        tempc = float(myTemperature - 400) / 10.0;
        tempc = (tempc - 32.0) * (5.0 / 9.0);

        messageID++;

        String myJSON;
        myJSON =  "{\"messageid\" : \"" + String(messageID) + "\", ";
        myJSON +=  "\"time\" : \"\", ";
        myJSON +=  "\"model\" : \"SwitchDoc Labs FT0300 AIO\", ";
        myJSON +=  "\"device\" : \"" + String(device) + "\", ";
        myJSON +=  "\"modelnumber\" : \"12\", ";
        myJSON +=  "\"battery\" : \"" + myBattery + "\", ";
        myJSON +=  "\"avewindspeed\" : \"" + String(myAveWindSpeed) + "\", ";
        myJSON +=  "\"gustwindspeed\" : \"" + String(myGust) + "\", ";
        myJSON +=  "\"winddirection\" : \"" + String( myWindDirection) + "\", ";
        myJSON +=  "\"cumulativerain\" : \"" + String(myCumulativeRain) + "\", ";
        myJSON +=  "\"temperature\" : \"" + String(tempc) + "\", ";
        myJSON +=  "\"humidity\" : \"" + String(myHumidity) + "\", ";
        myJSON +=  "\"light\" : \"" + String(myLight) + "\", ";
        myJSON +=  "\"uv\" : \"" + String(myUV) + "\", ";
        myJSON +=  "\"CRC\" : \"" + String(myCalculated, HEX) + "\"}";
        return myJSON;

      }
      else
      {
        Serial.println("F0300 Bad CRC");
      }

    }


  } // set to 16 as dataype = 4C

  return ErrorJSON;
}

void SDL_ESP32_WeatherRack2::eraseManchester()
{
  for ( int j = 0; j < 4; j++)
  {
    manchester[j] = j;
  }
}


uint8_t SDL_ESP32_WeatherRack2::Checksum(int length, uint8_t *buff)
{
  uint8_t mask = 0x7C;
  uint8_t checksum = 0x64;
  uint8_t data;
  int byteCnt;

  for ( byteCnt = 0; byteCnt < length; byteCnt++)
  {
    int bitCnt;
    data = buff[byteCnt];

    for ( bitCnt = 7; bitCnt >= 0 ; bitCnt-- )
    {
      uint8_t bit;

      // Rotate mask right
      bit = mask & 1;
      mask =  (mask >> 1 ) | (mask << 7);
      if ( bit )
      {
        mask ^= 0x18;
      }

      // XOR mask into checksum if data bit is 1
      if ( data & 0x80 )
      {
        checksum ^= mask;
      }
      data <<= 1;
    }
  }
  return checksum;
}

//============================================================================================
// CRC table
//============================================================================================
const unsigned char crc_table[256] = {
  0x00, 0x31, 0x62, 0x53, 0xc4, 0xf5, 0xa6, 0x97, 0xb9, 0x88, 0xdb, 0xea, 0x7d, 0x4c, 0x1f, 0x2e,
  0x43, 0x72, 0x21, 0x10, 0x87, 0xb6, 0xe5, 0xd4, 0xfa, 0xcb, 0x98, 0xa9, 0x3e, 0x0f, 0x5c, 0x6d,
  0x86, 0xb7, 0xe4, 0xd5, 0x42, 0x73, 0x20, 0x11, 0x3f, 0x0e, 0x5d, 0x6c, 0xfb, 0xca, 0x99, 0xa8,
  0xc5, 0xf4, 0xa7, 0x96, 0x01, 0x30, 0x63, 0x52, 0x7c, 0x4d, 0x1e, 0x2f, 0xb8, 0x89, 0xda, 0xeb,
  0x3d, 0x0c, 0x5f, 0x6e, 0xf9, 0xc8, 0x9b, 0xaa, 0x84, 0xb5, 0xe6, 0xd7, 0x40, 0x71, 0x22, 0x13,
  0x7e, 0x4f, 0x1c, 0x2d, 0xba, 0x8b, 0xd8, 0xe9, 0xc7, 0xf6, 0xa5, 0x94, 0x03, 0x32, 0x61, 0x50,
  0xbb, 0x8a, 0xd9, 0xe8, 0x7f, 0x4e, 0x1d, 0x2c, 0x02, 0x33, 0x60, 0x51, 0xc6, 0xf7, 0xa4, 0x95,
  0xf8, 0xc9, 0x9a, 0xab, 0x3c, 0x0d, 0x5e, 0x6f, 0x41, 0x70, 0x23, 0x12, 0x85, 0xb4, 0xe7, 0xd6,
  0x7a, 0x4b, 0x18, 0x29, 0xbe, 0x8f, 0xdc, 0xed, 0xc3, 0xf2, 0xa1, 0x90, 0x07, 0x36, 0x65, 0x54,
  0x39, 0x08, 0x5b, 0x6a, 0xfd, 0xcc, 0x9f, 0xae, 0x80, 0xb1, 0xe2, 0xd3, 0x44, 0x75, 0x26, 0x17,
  0xfc, 0xcd, 0x9e, 0xaf, 0x38, 0x09, 0x5a, 0x6b, 0x45, 0x74, 0x27, 0x16, 0x81, 0xb0, 0xe3, 0xd2,
  0xbf, 0x8e, 0xdd, 0xec, 0x7b, 0x4a, 0x19, 0x28, 0x06, 0x37, 0x64, 0x55, 0xc2, 0xf3, 0xa0, 0x91,
  0x47, 0x76, 0x25, 0x14, 0x83, 0xb2, 0xe1, 0xd0, 0xfe, 0xcf, 0x9c, 0xad, 0x3a, 0x0b, 0x58, 0x69,
  0x04, 0x35, 0x66, 0x57, 0xc0, 0xf1, 0xa2, 0x93, 0xbd, 0x8c, 0xdf, 0xee, 0x79, 0x48, 0x1b, 0x2a,
  0xc1, 0xf0, 0xa3, 0x92, 0x05, 0x34, 0x67, 0x56, 0x78, 0x49, 0x1a, 0x2b, 0xbc, 0x8d, 0xde, 0xef,
  0x82, 0xb3, 0xe0, 0xd1, 0x46, 0x77, 0x24, 0x15, 0x3b, 0x0a, 0x59, 0x68, 0xff, 0xce, 0x9d, 0xac
};

//============================================================================================
// Function: Calculate the CRC value
// Input:    crc: CRC initial value. lpBuff: string. ucLen: string length
// Return:   CRC value
//============================================================================================
uint8_t SDL_ESP32_WeatherRack2::GetCRC(uint8_t crc, uint8_t * lpBuff, uint8_t ucLen)
{
  while (ucLen)
  {
    ucLen--;
    crc = crc_table[*lpBuff ^ crc];
    lpBuff++;
  }
  return crc;
}
