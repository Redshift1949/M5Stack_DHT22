# M5Stack_DHT22
M5Stack using Arduino IDE displays temperature/humidity data from a DHT22 in graphical form and stores data on an SD card. The data is on a rolling display which shows the lastest 200 readings.

The data is stored with a time stamp which is retrieved from an NTP server. This requires a WiFi connection. If no WiFi is available the data is stored without a time stamp. The sample rate in my app is 1 reading/min but can easily be changed.

The temperature displayed is between 10-30C for my application but can be easily changed.

If buttons A and C are pressed at reset, the data file will be cleared ready to start again.
On power up the legend will display the temperature. Pressing the B button will display the humidity legend. Pressing button A will revert to temperature
