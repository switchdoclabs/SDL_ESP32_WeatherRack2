
/*
  SDL_ESP32_WeatherRack2
  Example for ESP32
  September 2020

*/


#include "SDL_ESP32_WeatherRack2.h"

SDL_ESP32_WeatherRack2 weatherRack2;




void setup()
{
  Serial.begin(115200);
  Serial.println("-----------");
  Serial.println("SwitchDoc Labs");
  Serial.println("WeatherRack2 and Indoor T/H Test"); \
  Serial.println("-----------");

  weatherRack2 = SDL_ESP32_WeatherRack2(600, true, true );

  weatherRack2.begin();

}



void loop()
{

  weatherRack2.waitForNextJSON();

  Serial.print("CurrentJSON=");
  Serial.println(weatherRack2.getCurrentJSON());

  Serial.print("Headers Found=");
  Serial.println(weatherRack2.readHeadersFound());
  Serial.print("WeatherRack2 Sensors Found=");
  Serial.println(weatherRack2.readWeatherRack2Found());
  Serial.print("Indoor T/H Found=");
  Serial.println(weatherRack2.readSDLIndoorTHFound());
  Serial.println();
  delay(5000);

}
