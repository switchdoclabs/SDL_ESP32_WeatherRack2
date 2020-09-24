#SDL_ESP32_WeatherRack2 <BR>
<BR>
SwitchDoc Labs<BR>
September 2020 <BR>


September 2020 Version 1.0 - Initial Release<BR>

This library and example program is designed to read 433Mhz signals from the SwitchDoc Labs WeatherRack2 Outdoor Weather Sensors.<BR>

https://shop.switchdoc.com/products/wireless-weatherrack2   <BR>

It also supports the SwitchDoc Labs Wireless Indoor Temperature and Humidity Sensor (all 8 channels).<BR>

https://shop.switchdoc.com/products/wireless-inside-temperature-and-humidity-sensor-f007th   <BR>


Any ASK/OOK 433MHz Reciever should work with this system.   We have used several, but recommend the following for a good inexpensive one:

RXB6 433MHz Superheterodyne Wireless Receiver Module for Arduino/ARM/AVR<BR>

https://amzn.to/33WC1sr

Connect the Data Output to Pin 32 on the ESP32.

Connect a 17cm single wire to the Antenna output.
Connect a 17cm single wire to the GND next to the Antenna output.

This forms a simple 1/4 wavelength dipole antenna.

#JSON Formats: <BR>

#WeatherRack2:<BR>

{"messageid" : "17", "time" : "", "model" : "SwitchDoc Labs FT0300 AIO", "device" : "41", "modelnumber" : "12", "battery" : "OK", "avewindspeed" : "6", "gustwindspeed" : "10", "winddirection" : "322", "cumulativerain" : "1422", "temperature" : "18.83", "humidity" : "44", "light" : "14871", "uv" : "8", "CRC" : "96"}<BR>

Fields

messageid : increments by one for each message (WeatherRack2 or Indoor T/H sensor) received and CRC verified<BR>
time: If you have set up the real time clock, it would be good to insert the time in this slot<BR>
model:  Description of the Sensor received<BR>
device: Serial Number of the sensor - changed on powerup but can be used to discriminate from other similar sensors in the area<BR>
modelnumber:   Sensor Model Number<BR>
battery:  OK if battery good, LOW if battery is getting low<BR>
avewindspeed: Average Wind Speed in m/s<BR>
gustwindspeed: Last Gust Speed in m/s<BR>
winddirection: Wind Direction in degrees from 0-359.<BR>
cumulativerain:  Total rain since last reset or power off.  in mm.<BR>
temperature:  outside temperature in C.
humidity:  Relative Humidity in %. <BR>
light:  Visible Sunlight in lux. <BR>
uv:  UV Index * 10 (meaning an uv index of 0.8 is in the example message above)<BR>
CRC: Calculated CRC value (It will match the CRC off of the message or you would not have received the message)<BR>

#SwitchDoc Labs Indoor T/H Sensor

{"messageid" : "13", "time" : "", "model" : "SwitchDoc Labs F007TH Thermo-Hygrometer", "device" : "54", "modelnumber" : "5", "channel" : "4", "battery" : "OK", "temperature" : "25.39", "humidity" : "38", "CRC" : "f"}

Fields

messageid : increments by one for each message (WeatherRack2 or Indoor T/H sensor) received and CRC verified<BR>
time: If you have set up the real time clock, it would be good to insert the time in this slot<BR>
model:  Description of the Sensor received<BR>
device: Serial Number of the sensor - changed on powerup but can be used to discriminate from other similar sensors in the area<BR>
modelnumber:   Sensor Model Number<BR>
battery:  OK if battery good, LOW if battery is getting low<BR>
temperature:  outside temperature in C.
humidity:  Relative Humidity in %. <BR>
CRC: Calculated CRC value (It will match the CRC off of the message or you would not have received the message)<BR>

More information on www.switchdoc.com



