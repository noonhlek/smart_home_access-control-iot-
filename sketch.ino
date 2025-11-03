#define BLYNK_TEMPLATE_ID "TMPL2KvveO7AB"
#define BLYNK_TEMPLATE_NAME "smart acess control"
#define BLYNK_AUTH_TOKEN "tj6FQlnbFgbJpHEp1ne9XsFTIWLvxccA"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Keypad.h>
#include <ESP32Servo.h>

// === WiFi ===
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// === Components ===
int servoPin = 21;  // Servo connected to pin 21
Servo door;

int pirPin = 22;
int buzzerPin = 27;
int redLED = 26;
int greenLED = 25;

// === Access Control ===
String authorizedID = "1234";
String guestOTP = "5678"; // generate a nw one after use 
bool otpUsed = false;
int failCount = 0;
bool alarmActive = false;
int currentHour = 10; // simulated clock

// === Virtual Pins ===
#define VPIN_MOTION V1
#define VPIN_LOGS V2
#define VPIN_ALERTS V3
#define VPIN_FAILCOUNT V4
#define VPIN_DOOR V5
#define VPIN_CANCELALARM V7

// === Keypad ===
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {19,18,5,17};
byte colPins[COLS] = {16,4,0,2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// === Buzzer Functions ===
void alarmOn() { tone(buzzerPin, 1000); }
void alarmOff() { noTone(buzzerPin); alarmActive = false; }
void keyBeep() { tone(buzzerPin, 2000); delay(50); noTone(buzzerPin); }
void deniedBeep() { for (int i = 0; i < 2; i++) { tone(buzzerPin, 500); delay(150); noTone(buzzerPin); delay(100); } }

// === Blynk callback: Cancel alarm switch ===
BLYNK_WRITE(VPIN_CANCELALARM) {
  int state = param.asInt();
  if (state == 1) {
    alarmOff();
    Blynk.virtualWrite(VPIN_ALERTS, "🚨 Alarm canceled remotely");
    Blynk.logEvent("alarm_on", "Alarm canceled remotely");
  }
}

// === Servo door control variables ===
bool doorMoving = false;
unsigned long doorStartTime = 0;
const unsigned long doorOpenDuration = 2000; // 2 seconds

void setup() {
  Serial.begin(115200);
  door.attach(servoPin); // Use the defined servo pin
  pinMode(pirPin, INPUT);
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

// === Main loop ===
void loop() {
  Blynk.run();

  if (alarmActive) alarmOn();

  // Motion detection
  int motion = digitalRead(pirPin);
  Blynk.virtualWrite(VPIN_MOTION, motion);
  if (motion == HIGH) {
    Serial.println("Motion detected!");
    if (currentHour < 7 || currentHour > 21) {
      alarmOn();
      alarmActive = true;
      Blynk.virtualWrite(VPIN_ALERTS, "⚠ Motion outside allowed hours");
      Blynk.logEvent("alarm_on", "Motion detected outside allowed hours");
    }
  }

  // Keypad input
  char key = keypad.getKey();
  if (key) {
    keyBeep();
    static String input = "";
    if (key == '#') {
      checkAccess(input);
      input = "";
    } else if (key == '*') {
      alarmOff();
      Blynk.virtualWrite(VPIN_ALERTS, "🚨 Alarm canceled by keypad");
      Blynk.logEvent("alarm_on", "Alarm canceled by keypad");
    } else {
      input += key;
    }
  }

  // Update servo door state
  updateDoor();
}

// === Access control functions ===
void checkAccess(String code) {
  if (currentHour < 7 || currentHour > 21) {
    denyAccess("Denied: Outside hours");
    return;
  }

  if (code == authorizedID) {
    grantAccess("Main User");
  } else if (code == guestOTP && !otpUsed) {
    grantAccess("Guest OTP");
    otpUsed = true;
  } else {
    failCount++;
    Blynk.virtualWrite(VPIN_FAILCOUNT, failCount);
    denyAccess("Denied: Wrong code");
    if (failCount >= 3) {
      alarmActive = true;
      Blynk.virtualWrite(VPIN_ALERTS, "⚠ Escalation: 3 failed attempts!");
      Blynk.logEvent("alarm_on", "Escalation: 3 failed attempts!");
    }
  }
}

void grantAccess(String userType) {
  Serial.println("Access granted: " + userType);
  Blynk.virtualWrite(VPIN_LOGS, "✅ Access granted: " + userType);

  // Turn on green LED, turn off red LED, beep
  digitalWrite(greenLED, HIGH);
  digitalWrite(redLED, LOW);
  tone(buzzerPin, 1500, 200);

  // Start moving the door
  door.write(90);
  Blynk.virtualWrite(VPIN_DOOR, 1);
  doorMoving = true;
  doorStartTime = millis();

  failCount = 0;
  Blynk.virtualWrite(VPIN_FAILCOUNT, failCount);
  alarmOff();
}

void denyAccess(String reason) {
  Serial.println(reason);
  Blynk.virtualWrite(VPIN_LOGS, "❌ " + reason);
  digitalWrite(redLED, HIGH);
  digitalWrite(greenLED, LOW);
  deniedBeep();
  delay(200);
  digitalWrite(redLED, LOW);
}

// === Non-blocking servo update ===
void updateDoor() {
  if (doorMoving) {
    if (millis() - doorStartTime >= doorOpenDuration) {
      door.write(0); // Close door
      Blynk.virtualWrite(VPIN_DOOR, 0);
      digitalWrite(greenLED, LOW);
      doorMoving = false; // stop updating
    }
  }
}
