#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

RTC_DS1307 rtc;

const int relayPin1 = 3;
const int relayPin2 = 4;
const int relayPin3 = 5;
const int relayPin4 = 6;

const int rtcPowerPin = 12;

const int wateringDuration = 10000;
const int timePrintInterval = 1000;
const int schedulePrintInterval = 5000;
const int ipPrintInterval = 10000;

unsigned long previousTimePrintMillis = 0;
unsigned long previousSchedulePrintMillis = 0;
unsigned long previousIpPrintMillis = 0;
unsigned long wateringStartTime = 0;

bool manualWateringRequested = false;
int selectedPump = 0;

char ssid[] = "ssid";
char pass[] = "pass";

WiFiServer server(80);

void setup() {
  Serial.begin(9600);

  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(relayPin3, OUTPUT);
  pinMode(relayPin4, OUTPUT);

  pinMode(rtcPowerPin, OUTPUT);
  digitalWrite(rtcPowerPin, HIGH);

  Wire.begin();
  rtc.begin();
  rtc.adjust(DateTime(__DATE__, __TIME__));

  stopWatering();

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("Local IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  DateTime now = rtc.now();

  if (now.hour() == 9 && now.minute() == 30 && now.second() == 0) {
    startWatering();
    Serial.println("Pumps on");
  }

  if (now.hour() == 15 && now.minute() == 30 && now.second() == 0) {
    startWatering();
    Serial.println("Pumps on");
  }

  if (manualWateringRequested) {
    if (selectedPump == 0) {
      startWatering();
      Serial.print("Pump ");
      Serial.print(selectedPump);
      Serial.println(" on");
    } else if (selectedPump == 1) {
      startWateringForPump(relayPin1);
      Serial.print("Pump ");
      Serial.print(selectedPump);
      Serial.println(" on");
    } else if (selectedPump == 2) {
      startWateringForPump(relayPin2);
      Serial.print("Pump ");
      Serial.print(selectedPump);
      Serial.println(" on");
    } else if (selectedPump == 3) {
      startWateringForPump(relayPin3);
      Serial.print("Pump ");
      Serial.print(selectedPump);
      Serial.println(" on");
    } else if (selectedPump == 4) {
      startWateringForPump(relayPin4);
      Serial.print("Pump ");
      Serial.print(selectedPump);
      Serial.println(" on");
    }

    manualWateringRequested = false;
    selectedPump = 0;
  }

  if (wateringStartTime > 0 && millis() - wateringStartTime >= wateringDuration) {
    stopWatering();
    wateringStartTime = 0;
    Serial.println("Pumps off");
  }

  if (millis() - previousTimePrintMillis >= timePrintInterval) {
    printTime();
    previousTimePrintMillis = millis();
  }

  if (millis() - previousSchedulePrintMillis >= schedulePrintInterval) {
    printSchedule();
    previousSchedulePrintMillis = millis();
  }

  if (millis() - previousIpPrintMillis >= ipPrintInterval) {
    Serial.print("Local IP address: ");
    Serial.println(WiFi.localIP());
    previousIpPrintMillis = millis();
  }

  WiFiClient client = server.available();
  if (client) {
    handleClient(client);
  }

  delay(1000);
}

void handleClient(WiFiClient client) {
  String request = client.readStringUntil('\r');
  client.flush();

  if (request.indexOf("/ ") != -1) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<html><head>");
    client.println("<script>");
    client.println("function updateTime() {");
    client.println("  var xhttp = new XMLHttpRequest();");
    client.println("  xhttp.onreadystatechange = function() {");
    client.println("    if (this.readyState == 4 && this.status == 200) {");
    client.println("      document.getElementById('time').innerHTML = this.responseText;");
    client.println("    }");
    client.println("  };");
    client.println("  xhttp.open('GET', '/getTime', true);");
    client.println("  xhttp.send();");
    client.println("}");
    client.println("setInterval(updateTime, 1000);");
    client.println("</script>");
    client.println("</head><body>");
    client.println("<h1>Welcome to the Watering System</h1>");
    client.println("<p>Current Time: <span id='time'>" + getCurrentTime() + "</span></p>");
    client.println("<p><a href='/water/1'>Start Pump 1</a></p>");
    client.println("<p><a href='/water/2'>Start Pump 2</a></p>");
    client.println("<p><a href='/water/3'>Start Pump 3</a></p>");
    client.println("<p><a href='/water/4'>Start Pump 4</a></p>");
    client.println("<p><a href='/stop'>Stop Pump</a></p>");
    client.println("</body></html>");
  }

  else if (request.indexOf("/water/") != -1) {
    int pump = getPumpFromRequest(request);
    if (pump != -1) {
      startWateringForPump(getRelayPinFromPump(pump));
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println();
      client.println("Pump " + String(pump) + " started.");
    } else {
      client.println("HTTP/1.1 404 Not Found");
      client.println("Content-Type: text/plain");
      client.println();
      client.println("Pump not found.");
    }
  }

  else if (request.indexOf("/stop") != -1) {
    stopWatering();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("Pumps stopped.");
  }

  else if (request.indexOf("/getTime") != -1) {
    getTime(client);
  }

  else {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("Not found.");
  }

  client.stop();
}

String getCurrentTime() {
  DateTime now = rtc.now();
  return String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
}

int getPumpFromRequest(String request) {
  int pump = -1;
  int index = request.indexOf("/water/");
  if (index != -1) {
    pump = request.substring(index + 7).toInt();
  }
  return pump;
}

int getRelayPinFromPump(int pump) {
  switch (pump) {
    case 1:
      return relayPin1;
    case 2:
      return relayPin2;
    case 3:
      return relayPin3;
    case 4:
      return relayPin4;
    default:
      return -1;
  }
}

void startWatering() {
  digitalWrite(relayPin1, LOW);
  digitalWrite(relayPin2, LOW);
  digitalWrite(relayPin3, LOW);
  digitalWrite(relayPin4, LOW);
  wateringStartTime = millis();
}

void startWateringForPump(int pumpPin) {
  digitalWrite(pumpPin, LOW);
  wateringStartTime = millis();
}

void stopWatering() {
  digitalWrite(relayPin1, HIGH);
  digitalWrite(relayPin2, HIGH);
  digitalWrite(relayPin3, HIGH);
  digitalWrite(relayPin4, HIGH);
  wateringStartTime = 0;
}

void printTime() {
  DateTime now = rtc.now();
  Serial.print("Current Time: ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.print(now.second());
  Serial.println();
}

void printSchedule() {
  Serial.println("Scheduled Watering Times:");
  printWateringTime(9, 30, 0);
  printWateringTime(15, 30, 0);
  Serial.println();
}

void printWateringTime(int hour, int minute, int second) {
  Serial.print("Watering Time: ");
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.print(second);
  Serial.println();
}

void getTime(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println();
  client.print(getCurrentTime());
}