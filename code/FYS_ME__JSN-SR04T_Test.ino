#include "arduino_secrets.h"

//Hi, this is 'stolen' code that has been modified for water depth measuring mode
//Originally, the code had some comms mechanisms, I commented them out but feel free to look


/*#include <WiFi.h>
#include <WebServer.h>*/

#define TRIG_PIN 5
#define ECHO_PIN 18

//WebServer server(80);

// Distance measurement 
float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.1500 / 2;  //Distance equation with 1500m/s speed of sound for water
  return distance;
}

/* Webpage handler
void handleRoot() {
  float dist = readDistance();
  String page = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='2'/>"
                "<title>ESP32 Water Distance</title>"
                "<style>h2 { font-size: 48px; }</style>" 
                "</head><body>"
                "<h2>Distance: " + String(dist, 2) + " cm</h2>"
                "</body></html>";
  server.send(200, "text/html", page);
}*/

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Start AP mode
  //WiFi.softAP("ESP32");

  /*Serial.println("Access Point Started");
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP()); // usually 192.168.4.1
*/

  /*
  // Start Web Server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started");*/
}

void loop() {
  //server.handleClient();
}