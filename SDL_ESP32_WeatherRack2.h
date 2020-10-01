//
//   SDL_ESP32_WeatherRack2.h
//   Version 1.2
//   SwitchDoc Labs   September 2020
//
//

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif


#define WEATHERRACK2_TIMEOUT 500
#define READ_WEATHERRACK2 true
#define READ_SDL_INDOOR_TH true

#define RX_IN_PIN 32

class SDL_ESP32_WeatherRack2 {
  public:
    SDL_ESP32_WeatherRack2(long timeout = WEATHERRACK2_TIMEOUT, boolean read_weatherrack2 = READ_WEATHERRACK2, boolean read_indoorth = READ_SDL_INDOOR_TH  );


    void begin(void);
    String getCurrentJSON();
    String waitForNextJSON();
    void setTimeout(long my_timeout);
    void set_ReadWeatherRack2(boolean my_read_weatherrack2);
    void set_ReadIndoorth(boolean my_readindoorth);
    long readHeadersFound();
    long readWeatherRack2Found();
    long readSDLIndoorTHFound();

    long _timeout;
    boolean _read_weatherrack2;
    boolean _read_indoorth;



  private:

    String findNextMessage();
    String currentJSON;
    String ErrorJSON;
    String TimeOutJSON;
    long messageID;


    long headersFound;
    long FT300MessagesFound;
    long FT007MessagesFound;




    String add(byte bitData);
    uint8_t Checksum(int length, uint8_t *buff);
    uint8_t GetCRC(uint8_t crc, uint8_t * lpBuff, uint8_t ucLen);
    String returnMessageJSON();
    void eraseManchester();



};
