#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Servo.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <SimpleTimer.h>

const char *ssid = "ib4tl";
const char *password = "allyourbase";

// Basic variables
ESP8266WebServer server(80);
Servo myservo;
int pos = 90;
const int led = 13;
char temp[3000];

// Time variables
//const char* ntpServerName = "time.nist.gov";
const char* ntpServerName = "pool.ntp.org";
const int NTP_PACKET_SIZE = 48;              // NTP time stamp is in the first 48 bytes of the message

unsigned int localPort = 2390;               // local port to listen for UDP packets
IPAddress timeServerIP;                      // time.nist.gov NTP server address
byte packetBuffer[ NTP_PACKET_SIZE];         //buffer to hold incoming and outgoing packets
WiFiUDP udp;                                 // A UDP instance to let us send and receive packets over UDP
//const int timeZone = 1;   // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
const int timeZone = -4;    // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)
SimpleTimer timer; // timer to get the time from NTP server

// Pump specific stuff
boolean isPoolPumpOn = false;
int onTimeAM = 5;  // On at 5am
int offTimeAM = 9; // Off at 9am
int onTimePM = 17; // On at 5pm
int offTimePM = 21; // Off at 9pm;
boolean isManualMode = false;

void setup(void)
{
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);

  // Wifi connection
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // UPD
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  // Webserver
  if (MDNS.begin("esp8266"))
  {
    Serial.println("MDNS responder started");
  }
  server.on("/", handleRoot);
  server.on("/on", turnSwitchOnCb);
  server.on("/off", turnSwitchOffCb);
  server.on("/manual", manualModeCb);
  server.on("/auto", autoModeCb);
  server.on("/status", displayStatus);
  server.on("/updatetime", acquireTime);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  // Servo
  myservo.attach(2);
  Serial.println("servo attached");
  myservo.write(pos);

  // initialize time
  acquireTime();

  // set timer to update time every 24 hours
  timer.setInterval(24 * 60 * 60 * 1000, acquireTime);

  // set the pool pump on state based on current time
  initPoolPump();
}

void loop(void)
{
  timer.run();
  server.handleClient();
  handlePoolPump();
}

void handlePoolPump()
{
  if (isManualMode)
  {
    return;
  }

  int thisHour = hour();

  if ((thisHour >= onTimeAM && thisHour < offTimeAM) || (thisHour >= onTimePM && thisHour < offTimePM))
  {
    // The pool pump should be running
    if (!isPoolPumpOn)
    {
      moveServoToOnPosition();
    }
  }
  else
  {
    // The pool pump should not be running.
    if (isPoolPumpOn)
    {
      moveServoToOffPosition();
    }
  }
}

void initPoolPump()
{
  int thisHour = hour();
  if ((thisHour >= onTimeAM && thisHour < offTimeAM) || (thisHour >= onTimePM && thisHour < offTimePM))
  {
    // The pool pump should be running
    moveServoToOnPosition();
  }
  else
  {
    // The pool pump should not be running.
    moveServoToOffPosition();
  }
}

void handleRoot()
{
  digitalWrite(led, 1);
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 3000,
           "<!DOCTYPE html>\
<html lang=\"en\">\
  <head>\
    <meta charset=\"utf-8\">\
    <meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
    <title>Pool pump</title>    \
    <link href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/css/bootstrap.min.css\" rel=\"stylesheet\">\
    <!--[if lt IE 9]>\
      <script src=\"https://oss.maxcdn.com/html5shiv/3.7.2/html5shiv.min.js\"></script>\
      <script src=\"https://oss.maxcdn.com/respond/1.4.2/respond.min.js\"></script>\
    <![endif]-->\
  </head>\
  <body>\
    <div class=\"container\">\
      <div class=\"row\">\
        <div class=\"col-md-12\">\
          <h1>Pool pump</h1>\
        </div>\
      </div>\
      <div class=\"row\">\
        <div class=\"col-md-12\">\
          <button id=\"on_button\" class=\"btn btn-success\">On</button>\
          <button id=\"off_button\" class=\"btn btn-danger\">Off</button>\
          <button id=\"manual_button\" class=\"btn btn-primary\">Manual</button>\
          <button id=\"auto_button\" class=\"btn btn-info\">Auto</button>\
          <button id=\"updatetime_button\" class=\"btn btn-warning\">Update time</button>\
        </div>\
      </div>\
      <hr/>\
      <div class=\"panel panel-info\">\
        <div class=\"panel-heading\">Current status</div>\
        <div class=\"panel-body\"></div>\
      </div>\
      <button id=\"refresh_button\" class=\"btn btn-default\">Refresh</button>\
    </div>\
    <script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.min.js\"></script>    \
    <script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/js/bootstrap.min.js\"></script>\
    <script>\
      $('#on_button').click(function(){\
        $('div.panel-body').html(\"\");\
        $.get('/on', function(data){\
          $('div.panel-body').html(data);\
        });\
      });\
      $('#off_button').click(function(){\
        $('div.panel-body').html(\"\");\
        $.get('/off', function(data){\
          $('div.panel-body').html(data);\
        });\
      });\
      $('#manual_button').click(function(){\
        $('div.panel-body').html(\"\");\
        $.get('/manual', function(data){\
          $('div.panel-body').html(data);\
        });\
      });\
      $('#auto_button').click(function(){\
        $('div.panel-body').html(\"\");\
        $.get('/auto', function(data){\
          $('div.panel-body').html(data);\
        });\
      });\
      $('#refresh_button').click(function(){\
        $('div.panel-body').html(\"\");\
        $.get('/status', function(data){\
          $('div.panel-body').html(data);\
        });\
      });\
      $('#updatetime_button').click(function(){\
        $('div.panel-body').html(\"\");\
        $.get('/status', function(data){\
          $('div.panel-body').html(data);\
        });\
      });\
    </script>\
  </body>\
</html>",

           // printf format strings
           hr,
           min % 60,
           sec % 60
          );
  server.send(200, "text/html", temp);
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void displayStatus()
{
  String message = getStatus();
  server.send(200, "text/html", message);
}

String getStatus()
{
  String message = "Currently configured time: ";
  message += hour();
  message += ":";
  message += minute();
  message += ":";
  message += second();
  message += " ";
  message += year();
  message += "-";
  message += month();
  message += "-";
  message += day();
  message += "<br/>";
  message += "Manual pool pump switching mode: ";
  message += isManualMode ? "true" : "false";
  message += "<br/>";
  message += "Is pool pump running: ";
  message += isPoolPumpOn ? "true" : "false";
  return message;
}

void moveServoToOffPosition(void)
{
  for (pos = 90; pos <= 170; pos += 1)    // goes from 90 degrees to 170 degrees
  { // in steps of 1 degree
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(15);                       // waits 15ms for the servo to reach the position
  }
  for (pos = 170; pos >= 90; pos -= 1)    // goes from 170 degrees to 90 degrees
  { // in steps of 1 degree
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(15);                       // waits 15ms for the servo to reach the position
    Serial.println(pos);
  }
  isPoolPumpOn = false;
}

void moveServoToOnPosition(void)
{
  for (pos = 90; pos >= 0; pos -= 1)    // goes from 90 degrees to 0 degrees
  { // in steps of 1 degree
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(15);                       // waits 15ms for the servo to reach the position
    Serial.println(pos);
  }
  for (pos = 0; pos <= 90; pos += 1)    // goes from 180 degrees to 0 degrees
  { // in steps of 1 degree
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(15);                       // waits 15ms for the servo to reach the position
    Serial.println(pos);
  }
  isPoolPumpOn = true;
}

void turnSwitchOnCb()
{
  isManualMode = true;
  moveServoToOnPosition();
  server.send(200, "text/html", "Pool pump ON!");
}

void turnSwitchOffCb()
{
  isManualMode = true;
  moveServoToOffPosition();
  server.send(200, "text/html", "Pool pump OFF!");
}

void manualModeCb()
{
  isManualMode = true;
  server.send(200, "text/html", "Manual pool pump switching ENABLED!");
}

void autoModeCb()
{
  isManualMode = false;
  server.send(200, "text/html", "Auto pool pump switching mode ENABLED!");
}

// Time functions
// Send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
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
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void acquireTime()
{
  time_t ntpTime = getNtpTime();
  setTime(ntpTime);
}

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  uint32_t beginWait = millis();

  while (millis() - beginWait < 1500)
  {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

