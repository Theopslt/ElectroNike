#include <Arduino.h>
#include <RTClib.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN 3
#define CS_PIN 4
#define DATA_PIN 5

#define LED_SECONDE 11
#define LED_AM 7
#define LED_PM 9
#define REVEIL_BUZZER 6
#define PM_AM_SWITCH 2
#define REVEIL_BUTTON 8
#define LED_ALARM 10

#define NOMBRE_TONE_REVEIL 6

unsigned long previousMinuteEdit = 0;

unsigned reveil = 1;
unsigned long previousReveilEdit = 0;
unsigned long previousReveilStart = 0;

unsigned alarmTonalites[NOMBRE_TONE_REVEIL] = {100, 100, 200, 500, 100, 100};
unsigned alarmTemps[NOMBRE_TONE_REVEIL * 2] = {100, 100, 100, 500, 100, 100, 100, 100, 100, 100, 100, 100};
unsigned alarmToneIndex = 0;
unsigned alarmTempsIndex = 0;
unsigned long alarmPreviousMillis = 0;
unsigned alarmState = 0;

MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

RTC_DS1307 RTC;

DateTime reveilTime;

bool formatHeure24 = true; // Variable pour suivre le format d'heure
bool lastSwitchState = LOW; // Suivre l'état précédent du bouton

void updateLedMatrix(DateTime time) {
  char heure[10];

  int switchState = digitalRead(PM_AM_SWITCH);

  if (switchState != lastSwitchState) {
    // Si l'état du bouton a changé, basculez le format de l'heure
    formatHeure24 = !formatHeure24;
    delay(200); // Délai pour éviter les rebonds
  }

  lastSwitchState = switchState;

  if (formatHeure24) {
    if (time.hour() >= 12) {
      digitalWrite(LED_AM, LOW);
      digitalWrite(LED_PM, HIGH);
    } else {
      digitalWrite(LED_AM, HIGH);
      digitalWrite(LED_PM, LOW);
    }

    sprintf(heure, "%02d:%02d", time.hour(), time.minute());
  } else {
    if (time.isPM()) {
      digitalWrite(LED_AM, LOW);
      digitalWrite(LED_PM, HIGH);
      sprintf(heure, "%02d:%02d PM", time.twelveHour(), time.minute());
    } else {
      digitalWrite(LED_AM, HIGH);
      digitalWrite(LED_PM, LOW);
      sprintf(heure, "%02d:%02d AM", time.twelveHour(), time.minute());
    }
  }

  Serial.println(heure);
  myDisplay.print(heure);
  previousMinuteEdit = millis();
}

void alarm(DateTime now, DateTime reveil, unsigned long timerDuration, unsigned long alarmDuration) {
  static unsigned long timerStartMillis = 0;
  static bool timerActive = false;
  static unsigned long alarmOnMillis = 0;

  if (!timerActive && now.hour() == reveil.hour() && now.minute() == reveil.minute()) {
    timerStartMillis = millis();
    timerActive = true;
    digitalWrite(LED_ALARM, HIGH);
  }

  if (timerActive && millis() - timerStartMillis >= timerDuration) {
    tone(REVEIL_BUZZER, alarmTonalites[alarmToneIndex]);
    alarmState = !alarmState;
    alarmPreviousMillis = millis();
    previousReveilStart = millis();
    timerActive = false;
    alarmOnMillis = millis();
  }

  if (alarmState == 1 && millis() - alarmPreviousMillis >= alarmTemps[alarmTempsIndex]) {
    Serial.println("Here3?");
    if (alarmToneIndex * 2 == alarmTempsIndex) {
      noTone(REVEIL_BUZZER);
    } else {
      alarmToneIndex++;
      tone(REVEIL_BUZZER, alarmTonalites[alarmToneIndex]);
    }

    if (alarmToneIndex == NOMBRE_TONE_REVEIL && alarmTempsIndex == NOMBRE_TONE_REVEIL * 2) {
      alarmToneIndex = 0;
      alarmTempsIndex = 0;
      alarmState = !alarmState;
    }

    alarmPreviousMillis = millis();
    alarmTempsIndex++;
  }

  // Éteindre l'alarme après 10 secondes
  if (alarmState == 1 && millis() - alarmOnMillis >= alarmDuration) {
    digitalWrite(LED_ALARM, LOW);
    alarmState = !alarmState;
  }
}

void toggleReveil() {
  reveil = !reveil;
  Serial.println((reveil ? "Réveil allumé" : "Réveil éteint"));
  if (reveil) {
    reveilTime = DateTime(2023, 1, 8, 21, 35);
  }
}

void setup() {
  Serial.begin(2400);
  Wire.begin();
  RTC.begin();

  if (!RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }

  previousMinuteEdit = millis();

  pinMode(LED_SECONDE, OUTPUT);
  pinMode(LED_AM, OUTPUT);
  pinMode(LED_PM, OUTPUT);
  pinMode(PM_AM_SWITCH, INPUT);
  pinMode(REVEIL_BUTTON, INPUT);
  pinMode(REVEIL_BUZZER, OUTPUT);
  pinMode(LED_ALARM, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(REVEIL_BUTTON), toggleReveil, RISING);

  cli();
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS12) | _BV(CS10);
  TIMSK1 = _BV(OCIE1A);
  TCNT1 = 0;
  OCR1A = (F_CPU / 1000 / 1024) * 500;
  sei();

  myDisplay.begin();
  myDisplay.setIntensity(0);
  myDisplay.displayClear();
  myDisplay.setTextAlignment(PA_CENTER);

  updateLedMatrix(RTC.now());
}

ISR(TIMER1_COMPA_vect) {
  digitalWrite(LED_SECONDE, !digitalRead(LED_SECONDE));
}

void loop() {
  DateTime now = RTC.now();

  if (millis() - previousMinuteEdit >= 20000) {
    updateLedMatrix(now);
  }

  DateTime reveil = DateTime(now.year(), now.month(), now.day(), 19, 30, 0);

  unsigned long timerDuration = 5000;
  unsigned long alarmDuration = 10000; // Durée de l'alarme allumée en millisecondes

  alarm(now, reveil, timerDuration, alarmDuration);
}