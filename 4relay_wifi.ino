#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

RTC_DS1307 rtc;

const int relayPin1 = 2;
const int relayPin2 = 3;
const int relayPin3 = 4;
const int relayPin4 = 5;

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

bool setTimeFlag = false; // Flag to indicate setting time from Serial Monitor

void setup() {
  Serial.begin(9600);

  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(relayPin3, OUTPUT);
  pinMode(relayPin4, OUTPUT);

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

  readSerialCommands(); // Check for commands from the Serial Monitor

  delay(1000);
}

void handleClient(WiFiClient client) {
  String request = client.readStringUntil('\r');
  client.flush();

  if (request.indexOf("/water/") != -1) {
    int pump = getPumpFromRequest(request);
    if (pump != -1) {
      startWateringForPump(getRelayPinFromPump(pump));
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println();
      client.println("Pump " + String(pump) + " started.");
      Serial.print("Pump ");
      Serial.print(pump);
      Serial.println(" on");
    } else {
      client.println("HTTP/1.1 404 Not Found");
      client.println("Content-Type: text/plain");
      client.println();
      client.println("Pump not found.");
    }
  } else if (request.indexOf("/stop") != -1) {
    stopWatering();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("Pumps stopped.");
    Serial.println("Pumps off");
  } else if (request.indexOf("/getCurrentTime") != -1) {
    getCurrentTime(client);
  } else if (request.indexOf("GET / ") != -1) { // Check for root URL
    sendMainPage(client);
  } else {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("Not found.");
  }

  client.stop();
}

void sendMainPage(WiFiClient client) {
  DateTime now = rtc.now();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.println("<html><head>");
  client.println("</head><body>");
  client.println("<h1>Welcome to the Watering System</h1>");
  client.print("Current Time: ");
  client.print(now.hour());
  client.print(":");
  client.print(now.minute());
  client.print(":");
  client.print(now.second());
  client.println("<br><br>");
  
  // Button links for each pump with JavaScript to make asynchronous requests
  for (int i = 1; i <= 4; i++) {
    client.println("<button onclick='startPump(" + String(i) + ")'>Start Pump " + String(i) + "</button> ");
  }

  // Stop button with JavaScript to make an asynchronous request
  client.println("<button onclick='stopPumps()'>Stop All Pumps</button>");

  // JavaScript function to start a pump
  client.println("<script>");
  client.println("function startPump(pump) {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.open('GET', '/water/' + pump, true);");
  client.println("  xhr.send();");
  client.println("}");
  
  // JavaScript function to stop all pumps
  client.println("function stopPumps() {");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.open('GET', '/stop', true);");
  client.println("  xhr.send();");
  client.println("}");
  client.println("</script>");

  client.println("</body></html>");
}

void getCurrentTime(WiFiClient client) {
  DateTime now = rtc.now();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println();
  client.print(now.hour());
  client.print(":");
  client.print(now.minute());
  client.print(":");
  client.print(now.second());
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
  Serial.println("Pumps off");
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

void readSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith("water")) {
      int pump = command.substring(5).toInt();
      if (pump >= 1 && pump <= 4) {
        startWateringForPump(getRelayPinFromPump(pump));
        Serial.println("Pump " + String(pump) + " started.");
      } else {
        Serial.println("Invalid pump number.");
      }
    } else if (command.equals("stop")) {
      stopWatering();
      Serial.println("Pumps stopped.");
    } else if (command.equals("time")) {
      setTimeFlag = true; // Set the flag to indicate time setting
      Serial.println("Enter a new time (HH MM SS):");
    } else if (setTimeFlag) {
      // If the flag is set, parse the time input
      if (command.length() == 8) {
        int hh = command.substring(0, 2).toInt();
        int mm = command.substring(3, 5).toInt();
        int ss = command.substring(6, 8).toInt();
        DateTime newDateTime = DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(), hh, mm, ss);
        rtc.adjust(newDateTime);
        Serial.println("Time set to: " + command);
        setTimeFlag = false; // Reset the flag
      } else {
        Serial.println("Invalid time format. Use HH MM SS.");
      }
    } else {
      Serial.println("Invalid command.");
    }
  }
}
