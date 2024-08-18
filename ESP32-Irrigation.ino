#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <PCF8575.h> //https://github.com/xreef/PCF8575_library/archive/master.zip
#include <Wire.h>
#include <NTPClient.h>
//#include <TimeLib.h>

const char *ssid = "";
const char *password = "";
PCF8575 pcf8575(0x20, 22,23);

String output26State = "off";
String output27State = "off";
const int output26 = 26;
const int output27 = 27;
WiFiServer server(80);
String header;
unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;

//Wathering
const unsigned long startupTime = millis() + 1000 * 60;
const unsigned int quantitySprinklers = 5;
short Sprinklers [quantitySprinklers];// od 1
bool watheringIsStarted = 0; 
unsigned long  watheringTimeOn= millis(); // czas podlewania dla danego zaworu
int podlewany=0;//zmienna informująca który zawór jest aktualnie podlewany
const int watherOFF = 0;
const int watherON = 1;
const int GODZINA = 3;
unsigned long czasCzekania;
constexpr unsigned long watheringStartupTime = 22*1000*60*60;//godzina 22 w ms
unsigned long setupTime=0;
unsigned long watheringTime=1000*60*15;
unsigned long CZASPODLEWANIA = quantitySprinklers * watheringTime;

//TimeServer
unsigned short czas[3] = {0,0,0};
String dateAndTime=" ";//date and time init




//functions
String getTime();
bool Rained()//check is wet
{
  return pcf8575.digitalRead(0);// jeśli trzeba będzie odwrotnie dodaj not (!)
}

bool isWatheringTime()
{
if(czasCzekania<=millis())//jeśli już czas to sprawdź czy odpalić test na podlewanie
{
 getTime();//aktualizuj date i czas
 czasCzekania=24*1000*60*60 + millis() - CZASPODLEWANIA;
 return true;
}
return false;
}



bool WatheringStart()// czy czas na rozpoczęcie podlewania + czy nie jest mokry czujnik 
{
  return Rained() * isWatheringTime();
}

void Wathering()//15min default
{
  watheringIsStarted=1;
  unsigned long now = millis();
  if(now - watheringTimeOn > watheringTime)
  {
  if(podlewany==quantitySprinklers)//sprawdzam czy to jest ostatni zawor
    {
    podlewany=0;//resetujemy na pierwszy zawór
    watheringIsStarted=false; // wylaczamy flage podlewania
    digitalWrite(Sprinklers[podlewany], watherOFF);//włączenie podlewania zaworu
    return ;
  }
    podlewany++;//zaczynamy od inkrementu by było na 0 - czyli pierwszy elemeenty tablicy
    digitalWrite(Sprinklers[podlewany], watherON);//włączenie podlewania zaworu
    if(podlewany==1)// jeśli pierwszy to nie wyłączamy poprzedniego
    return ;
    digitalWrite(Sprinklers[podlewany-1], watherOFF);
    

}
}





String getTime(){
const long utcOffsetInSeconds = 3600;

String daysOfTheWeek[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", utcOffsetInSeconds);


  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
  }
  timeClient.begin();
  timeClient.update();



  //Serial.println(timeClient.getFormattedTime());
  Serial.println(timeClient.getEpochTime());
  Serial.println(timeClient.getDay());
  
  // Konwersja epoch time na strukturę czasu
  struct tm *timeinfo;
  time_t epoch_time = timeClient.getEpochTime();
  timeinfo = localtime(&epoch_time);
  // Uzyskanie formatowanej daty i czasu
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);// mogę sobie uciąć samą datę bez godziny do dziennego API
  Serial.println("Obecna data: " + String(buffer));
  dateAndTime = buffer;

  // Uzyskanie daty dnia następnego
  timeinfo->tm_mday += 1;
  mktime(timeinfo);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
  Serial.println("Data dnia następnego: " + String(buffer));
  //jutro = String(buffer);

  // Uzyskanie daty dnia poprzedniego
  timeinfo->tm_mday -= 2; // Odejmujemy 2, aby uzyskać datę dnia poprzedniego
  mktime(timeinfo);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
  Serial.println("Data dnia poprzedniego: " + String(buffer));
 // wczoraj = String(buffer);
  czas[0]=timeClient.getHours();
  czas[1]=timeClient.getMinutes();
  czas[2]=timeClient.getSeconds();
  return (String)timeClient.getFormattedTime();

}


void setup() {
  Serial.begin(115200);
  //expander
  pcf8575.begin();
    for(int i=1;i<16;i++){
    pcf8575.pinMode(i, OUTPUT);
  }
  pcf8575.pinMode(0, INPUT);//expander pin 0 for wather sensor

    for(int i=0;i<16;i++){
    pcf8575.pinMode(i, OUTPUT);//All low
  }
//Web Server
  pinMode(output26, OUTPUT);
  pinMode(output27, OUTPUT);
  // Set outputs to LOW
  digitalWrite(output26, LOW);
  digitalWrite(output27, LOW);


//OTA
  
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //OTA

  server.begin();//Server

  if (MDNS.begin("esp8266")) { 
    Serial.println("MDNS responder started"); 
    }
  getTime();// zapisz czas
 
 
  //wylicz ile trzeba poczekać do godziny 21 (zobacz czy sie spieszy godzine czy późni)
  setupTime = czas[0] *60*60*1000 + czas[1] *60*1000 + czas[2] *1000;// w ms
  //godzina 22 w ms to -> 79 200 000
  if(setupTime>watheringStartupTime)// jest już po czasie
  {
    czasCzekania = 86400000 - (setupTime-watheringStartupTime)+millis(); // odjąć czas nadwyżki od 24h w ms
  }
  else//jest przed czasem
  {
    czasCzekania=watheringStartupTime-setupTime+millis(); //dodanie milis startowo aby potem odejmować od aktualnego milis i porównać wartości
  }

  //Time
}

void loop() {
  ArduinoOTA.handle();
//OTA
WiFiClient client = server.available();
 if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /26/on") >= 0) {
              Serial.println("GPIO 26 on");
              output26State = "on";
              digitalWrite(output26, HIGH);
            } else if (header.indexOf("GET /26/off") >= 0) {
              Serial.println("GPIO 26 off");
              output26State = "off";
              digitalWrite(output26, LOW);
            } else if (header.indexOf("GET /27/on") >= 0) {
              Serial.println("GPIO 27 on");
              output27State = "on";
              digitalWrite(output27, HIGH);
            } else if (header.indexOf("GET /27/off") >= 0) {
              Serial.println("GPIO 27 off");
              output27State = "off";
              digitalWrite(output27, LOW);
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");
            
            // Display current state, and ON/OFF buttons for GPIO 26  
            client.println("<p>GPIO 26 - State " + output26State + "</p>");
            // If the output26State is off, it displays the ON button       
            if (output26State=="off") {
              client.println("<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
            } 
               
            // Display current state, and ON/OFF buttons for GPIO 27  
            client.println("<p>GPIO 27 - State " + output27State + "</p>");
            // If the output27State is off, it displays the ON button       
            if (output27State=="off") {
              client.println("<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
//Web Server




//Wathering
if(WatheringStart() || watheringIsStarted){
  Wathering();
}
}


