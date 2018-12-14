// Display time,temperature and humidity on an M5Stack using a DHT22. 
// Time is retrieved from an ntp server and all temp and humidity is displayed on
// a graph which uses a circular buffer to keep the last 4 hours of data on a rolling display
// The default sampling time is 1min but is easily changed
// The legend on the left of the display only refers to the temperature.
// Multiple DHT22s could be added
// Next phase is to store the data on the microsd card with the time the reading was taken.
// As I use this in my house, only the temperatures between 10 and 30 are displayed. This can easily be changed
// If you wish to read the time you will need to have WiFi to access an NTP server.
// If WiFi is not available it will still run but there will be no time stamp 

// The DHT22 code was included from the SimpleDHT library https://github.com/winlinvip/SimpleDHT
// The circular buffer was included from Roberto Lo Giacco's library https://github.com/rlogiacco/CircularBuffer
// and of course the M5Stack library https://github.com/m5stack/M5Stack


#include <M5Stack.h>
#include <SimpleDHT.h>
#include <time.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <CircularBuffer.h>


// settings of the x and y axis
#define INITIAL_GRAPH_X_POS       30
#define INITIAL_GRAPH_Y_POS       3
#define GRAPH_HEIGHT              180
#define GRAPH_WIDTH               270

// Coordinates of temp and humidity
#define POS_X_DATA 30
#define POS_Y_DATA 200

// These are the 2 circular buffers for the temperature and humidity readings
CircularBuffer<float, GRAPH_WIDTH> temperatureBuffer;
CircularBuffer<float, GRAPH_WIDTH> humidityBuffer;

// Colours used
#define BLACK           0x0000
#define RED             0xF800
#define CYAN            0x07FF
#define YELLOW          0xFFE0 
#define WHITE           0xFFFF
#define GREY            0x5AEB 
 
#define DHTPIN 22 // pin 5 is the DHT signal pin on the M5Stack
 
// sensor object constructor
SimpleDHT22 dht;
 
// X position variable
int xPos = 1;
 
// Temp and humidity variables
float humidity = 0.0;
float temperature = 0.0;

int WiFiStatus = WL_IDLE_STATUS;

#define MYSSID                  // uncomment this and fill in your ssid and password
#ifdef MYSSID
#include "mySSIDandPassword.h"  // this is to prevent me accidently publishing my ssid and password :)
#else
char ssid[] = "***********";    //  your network SSID (name)
char password[] = "***********";       // your network password
#endif

int keyIndex = 0;            // your network key Index number (needed only for WEP)

unsigned int localPort = 2390;      // local port to listen for UDP packets

IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
 
const int chipSelect = 4;
char timeBuffer[20];
long previousMillis = 0;        // will store the time the last reading was taken

// the follow variables is a long because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long interval = 60000;           // interval at which to read the DHT22 (milliseconds) 


/**************************  SETUP  *******************************/
void setup(void) {
  int counter = 0;

    //Initialise the M5Stack
  M5.begin();
  
  Serial.begin(115200);
  delay(1000);
  Serial.println("Initializing SD card...");

    // erase the screen
  M5.Lcd.fillScreen(BLACK); 
  M5.Lcd.setTextSize(4);

  
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(50, 60);
    M5.Lcd.println("NO SD CARD");
    // don't do anything more:
    while (1);
  }
  Serial.println("card initialized.");

  if(M5.BtnA.read() && M5.BtnC.read())
    {
      // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open("/datalog.txt", FILE_WRITE);
  
    // if the file is available, write to it:
    if (dataFile) 
      {
      dataFile.println("X");
      dataFile.close();
      M5.Lcd.setTextColor(GREEN);
      M5.Lcd.setCursor(30, 60);
      M5.Lcd.println("File erased");
      delay(4000);
      }
    // if the file isn't open, pop up an error:
    else 
      {
      Serial.println("error opening datalog.txt");
      } 
    }
    
  M5.Lcd.fillScreen(BLACK);  
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setCursor(50, 60);
  M5.Lcd.println("Connecting");
  M5.Lcd.setCursor(90, 110);
  M5.Lcd.print("to WiFi");

  //*************************************************************************
  // attempt to connect to Wifi network:
  // 5 attempts to connect to WiFi, if there is no WiFi the time will not be displayed on the top of the display
  
  while (WiFiStatus != WL_CONNECTED && counter < 5) 
    {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFiStatus = WiFi.begin(ssid, password);
    counter++;
    // wait 10 seconds for connection:
    delay(10000);
    }
//*******************************************************************************

  if(WiFiStatus == WL_CONNECTED) // only if WiFi is connected
    {
    Serial.println("Connected to wifi");
    printWifiStatus();

    Serial.println("\nStarting connection to server...");
    Udp.begin(localPort);
    }
  else
    {
    M5.Lcd.setTextSize(2);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(40, 80); 
    M5.Lcd.println("WiFi NOT CONNECTED");
    M5.Lcd.println();
    M5.Lcd.println();
    M5.Lcd.println();
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.print(" Push button 1 to continue");
    
    while(!M5.BtnA.wasReleased()) // wait until button 1 has been pressed
      M5.update(); 
    }
    
    // erase the screen
  M5.Lcd.fillScreen(BLACK);
 
  // Draw the axis lines
  //drawFastVLine(x,y,width,color) --> linha vertical
  M5.Lcd.drawFastVLine(INITIAL_GRAPH_X_POS,INITIAL_GRAPH_Y_POS, GRAPH_HEIGHT,WHITE); 
  M5.Lcd.drawFastHLine(INITIAL_GRAPH_X_POS,GRAPH_HEIGHT+1,GRAPH_WIDTH,WHITE); 
  drawGrid();
       
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(POS_X_DATA-10, POS_Y_DATA);
  M5.Lcd.print("T: "); // temperature indicator
  M5.Lcd.setCursor(POS_X_DATA+85, POS_Y_DATA);
  M5.Lcd.print(" H: "); // humidity indicator
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setTextSize(1);

  displayTemperatureLegend();
  M5.Lcd.setTextSize(2);
  
}
/**************************************************************/


/***************************** LOOP **************************/
void loop() 
  {

  unsigned long currentMillis = millis();
  
  // Read the temperature and humidity from the DHT22
  float temp, humid;
  unsigned long epoch;

  if(M5.BtnA.read()) // if button A has been pressed
      {
      displayTemperatureLegend();
      while(M5.BtnA.read())
        M5.update();
      }
      
  if(M5.BtnB.read()) // if button B has been pressed
      {
      displayHumidityLegend();
      while(M5.BtnB.read())
        M5.update();
      }

  if(currentMillis - previousMillis > interval)
    {
      previousMillis = currentMillis; 
  
      int status = dht.read2(DHTPIN, &temp, &humid, NULL);
 
      if (status == SimpleDHTErrSuccess) 
        {
        temperature = temp;
        humidity = humid;
        }

  if(WiFiStatus == WL_CONNECTED) // only if WiFi is connected
    {
      sendNTPpacket(timeServer); // send an NTP packet to a time server
      // wait to see if a reply is available
      delay(1000);
      if (Udp.parsePacket()) {
        Serial.println("packet received");
        // We've received a packet, read the data from it
        Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    
        //the timestamp starts at byte 40 of the received packet and is four bytes,
        // or two words, long. First, esxtract the two words:
    
        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        // combine the four bytes (two words) into a long integer
        // this is NTP time (seconds since Jan 1 1900):
        unsigned long secsSince1900 = highWord << 16 | lowWord;
    
        // now convert NTP time into everyday time:
        // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
        const unsigned long seventyYears = 2208988800UL;
        // subtract seventy years:
        epoch = secsSince1900 - seventyYears;
    
        // print the hour, minute and second:
        Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
        Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
        Serial.print(':');
     
        if (((epoch % 3600) / 60) < 10) {
          // In the first 10 minutes of each hour, we'll want a leading '0'
          Serial.print('0');
        }
        Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
        Serial.print(':');
        if ((epoch % 60) < 10) {
          // In the first 10 seconds of each minute, we'll want a leading '0'
          Serial.print('0');
        }
        Serial.println(epoch % 60); // print the second 
        sprintf(timeBuffer, "%02d:%02d",(epoch  % 86400L) / 3600,(epoch  % 3600) / 60); 
      }
    }
  else
    delay(1000);
 
  // push the readings onto the circular buffer, when the buffer is full the oldest reading is lost
   temperatureBuffer.push(temperature);
   humidityBuffer.push(humidity);
   // update the graphs on the display
   updateGraphs();
   
   if(WiFiStatus == WL_CONNECTED) // only if WiFi is connected
      {
        M5.Lcd.fillRect(240, POS_Y_DATA, 80, 20, BLACK); // erase the time
        M5.Lcd.setCursor(240, POS_Y_DATA);
        M5.Lcd.setTextColor(YELLOW);
        M5.Lcd.print(timeBuffer);
      }
      
   saveDataToSdCard(temperature, humidity, timeBuffer);
   
 // increase the x position
 xPos++;
 
 // If xPos exceeds the width of the graph, reset it to 1.
 if(xPos == GRAPH_WIDTH)
 {
   xPos = 1; //reset the x counter  
 }
 
 // erase the area where the temperature and humidity are displayed, this prevents overwriting the text
  
 M5.Lcd.fillRect(POS_X_DATA+20, POS_Y_DATA, 60, 30, BLACK);
 M5.Lcd.fillRect(POS_X_DATA+125, POS_Y_DATA, 80, 30, BLACK);
 
//Place the cursor to write the temperature
  M5.Lcd.setCursor(POS_X_DATA+20, POS_Y_DATA);  
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.printf("%.1f%c",temperature,(char)247);
 
  //Place the cursor to write the humidity
  M5.Lcd.setCursor(POS_X_DATA+125, POS_Y_DATA);    
  M5.Lcd.setTextColor(CYAN);
  M5.Lcd.printf("%.1f%c",humidity,'%');

    }
 
}
/*****************************************************************************/

/***************************** Print WiFi Status *****************************/
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");

  M5.Lcd.fillScreen(BLACK); 
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(0, 100);
  M5.Lcd.print("Connected to: ");
  M5.Lcd.println(WiFi.SSID());
  M5.Lcd.println();
  M5.Lcd.println();
  M5.Lcd.setTextColor(RED);
  M5.Lcd.print("Push button 1 to continue");
  while(!M5.BtnA.wasReleased())
    M5.update();
}
/*******************************************************************************/


/********* send an NTP request to the time server at the given address *********/
unsigned long sendNTPpacket(IPAddress& address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);;
  Udp.endPacket();

}
/**********************************************************************/


/********************** Update the graph display with the circular buffers ************************/
void updateGraphs()
{
int t, h;

if(temperatureBuffer.isFull())
  {
  M5.Lcd.fillRect(INITIAL_GRAPH_X_POS+1, INITIAL_GRAPH_Y_POS-1, GRAPH_WIDTH, GRAPH_HEIGHT-1, BLACK);  
  drawGrid();
  }

if(!temperatureBuffer.isEmpty()) 
  {
  for (decltype(temperatureBuffer)::index_t i = 0; i < temperatureBuffer.size() - 1; i++) 
    {
     // it is neccessary to remap the float values into integers to a scale that can be displayed
     t = map(temperatureBuffer[i]*10,100,300,0,GRAPH_HEIGHT);
     M5.Lcd.drawPixel(INITIAL_GRAPH_X_POS+i, GRAPH_HEIGHT - t, GREEN); 
     h = map(humidityBuffer[i],0,100,0,GRAPH_HEIGHT);
     M5.Lcd.drawPixel(INITIAL_GRAPH_X_POS+i, GRAPH_HEIGHT-h, CYAN); 
    }
  }
   
}
/************************************************************************************************/



void saveDataToSdCard(float temperature, float humidity, char* time)
{
  // make a string for assembling the data to log:
  String dataString = "";

  if(WiFiStatus == WL_CONNECTED) // only if WiFi is connected
      {
      dataString += time;
      dataString += ',';
      }
      
  dataString += String(temperature);
  dataString += ",";
  dataString += String(humidity);   

   // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("/datalog.txt", FILE_APPEND);

  // if the file is available, write to it:
  if (dataFile) 
    {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
    }
  // if the file isn't open, pop up an error:
  else 
    {
    Serial.println("error opening datalog.txt");
    }
}


void drawGrid()
{
  M5.Lcd.drawFastHLine(INITIAL_GRAPH_X_POS,GRAPH_HEIGHT-43,GRAPH_WIDTH,GREY);
  M5.Lcd.drawFastHLine(INITIAL_GRAPH_X_POS,GRAPH_HEIGHT-89,GRAPH_WIDTH,GREY);
  M5.Lcd.drawFastHLine(INITIAL_GRAPH_X_POS,GRAPH_HEIGHT-135,GRAPH_WIDTH,GREY);
  M5.Lcd.drawFastVLine(INITIAL_GRAPH_X_POS+60,INITIAL_GRAPH_Y_POS, GRAPH_HEIGHT,GREY); 
  M5.Lcd.drawFastVLine(INITIAL_GRAPH_X_POS+120,INITIAL_GRAPH_Y_POS, GRAPH_HEIGHT,GREY); 
  M5.Lcd.drawFastVLine(INITIAL_GRAPH_X_POS+180,INITIAL_GRAPH_Y_POS, GRAPH_HEIGHT,GREY); 
  M5.Lcd.drawFastVLine(INITIAL_GRAPH_X_POS+240,INITIAL_GRAPH_Y_POS, GRAPH_HEIGHT,GREY); 
}



#define TEMPERATURE_VALUE_WIDTH 20
#define TEMPERATURE_VALUE_HEIGHT 10

void displayTemperatureLegend()
{
  // These are the positions of the temperature Y legend on the left of the display.
    M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setTextSize(1);
  M5.Lcd.fillRect(10, GRAPH_HEIGHT, TEMPERATURE_VALUE_WIDTH, TEMPERATURE_VALUE_HEIGHT, BLACK);
  M5.Lcd.setCursor(10,GRAPH_HEIGHT);
  M5.Lcd.print("10");
  M5.Lcd.fillRect(10, GRAPH_HEIGHT-46, TEMPERATURE_VALUE_WIDTH, TEMPERATURE_VALUE_HEIGHT, BLACK);
  M5.Lcd.setCursor(10,GRAPH_HEIGHT-46);
  M5.Lcd.print("15");
  M5.Lcd.fillRect(10, GRAPH_HEIGHT-92, TEMPERATURE_VALUE_WIDTH, TEMPERATURE_VALUE_HEIGHT, BLACK);
  M5.Lcd.setCursor(10,GRAPH_HEIGHT-92);
  M5.Lcd.print("20");
  M5.Lcd.fillRect(10, GRAPH_HEIGHT-138, TEMPERATURE_VALUE_WIDTH, TEMPERATURE_VALUE_HEIGHT, BLACK);
  M5.Lcd.setCursor(10,GRAPH_HEIGHT-138);
  M5.Lcd.print("25");
  M5.Lcd.setTextSize(2);
  
}

#define HUMIDITY_VALUE_WIDTH 20
#define HUMIDITY_VALUE_HEIGHT 10

void displayHumidityLegend()
{
  // These are the positions of the humidity Y legend on the left of the display.
  M5.Lcd.setTextColor(CYAN);
  M5.Lcd.setTextSize(1);
  M5.Lcd.fillRect(10, GRAPH_HEIGHT, HUMIDITY_VALUE_WIDTH , HUMIDITY_VALUE_HEIGHT, BLACK);
  M5.Lcd.setCursor(10,GRAPH_HEIGHT);
  M5.Lcd.print("25");
  M5.Lcd.fillRect(10, GRAPH_HEIGHT-46, HUMIDITY_VALUE_WIDTH, HUMIDITY_VALUE_HEIGHT, BLACK);
  M5.Lcd.setCursor(10,GRAPH_HEIGHT-46);
  M5.Lcd.print("35");
  M5.Lcd.fillRect(10, GRAPH_HEIGHT-92, HUMIDITY_VALUE_WIDTH, HUMIDITY_VALUE_HEIGHT, BLACK);
  M5.Lcd.setCursor(10,GRAPH_HEIGHT-92);
  M5.Lcd.print("45");
  M5.Lcd.fillRect(10, GRAPH_HEIGHT-138, HUMIDITY_VALUE_WIDTH, HUMIDITY_VALUE_HEIGHT, BLACK);
  M5.Lcd.setCursor(10,GRAPH_HEIGHT-138);
  M5.Lcd.print("55");
  M5.Lcd.setTextSize(2);
  
}
