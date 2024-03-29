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
#define TRIGPIN 10
#define ECOPIN 12

unsigned long previousMinuteEdit = 0;

unsigned reveil = 1;
unsigned long previousReveilEdit = 0;
unsigned long previousReveilStart = 0;

MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

RTC_DS1307 RTC;

DateTime reveilTime;

bool formatHeure24 = true; // Variable pour suivre le format d'heure
bool lastSwitchState = LOW; // Suivre l'état précédent du bouton
unsigned long alarmPreviousMillis = 0;
unsigned alarmState = 0;
int switchState = 0;
unsigned long timerStartMillis = 0;
bool timerActive = false;
unsigned long alarmOnMillis = 0;
int alarmPasse = 0;
int MesureMaxi = 10; // Distance maxi a mesurer //
int MesureMini = 3; // Distance mini a mesurer //
long Duree;
long Distance;
int mainDetecte=0;


void updateLedMatrix(DateTime time) {
  char heure[10];

  if (switchState != lastSwitchState) {
    // Si l'état du bouton a changé, basculez le format de l'heure
    formatHeure24 = !formatHeure24;
    delay(200); // Délai pour éviter les rebonds
  }
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
  if (!alarmPasse && !timerActive && now.hour() == reveil.hour() && now.minute() == reveil.minute()) {
    timerStartMillis = millis();
    timerActive = true;
    digitalWrite(REVEIL_BUZZER, HIGH);
    alarmState = 1;
  }

  if (timerActive && millis() - alarmPreviousMillis >= timerDuration) {
    digitalWrite(REVEIL_BUZZER, !digitalRead(REVEIL_BUZZER));
    alarmPreviousMillis = millis();
  }

  // Éteindre l'alarme après 20 secondes
  if (alarmState == 1 && millis() - timerStartMillis >= alarmDuration && mainDetecte == 1) {
    digitalWrite(REVEIL_BUZZER, LOW);
    alarmState = 0;
    timerActive = false; // Réinitialiser l'état du minuteur
    alarmPasse = 1;
  }

  if (millis() - timerStartMillis >= alarmDuration + 120000) {
    alarmPasse = 0;
  }
}

void toggleReveil() {
  reveil = !reveil;
  Serial.println((reveil ? "Réveil allumé" : "Réveil éteint"));
  if (reveil) {
    reveilTime = DateTime(2023, 1, 8, 21, 35);
  }
}


int detectionMain() {
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);

  // Mesure la durée de l'impulsion reçue sur la broche Echo
  Duree = pulseIn(ECOPIN, HIGH);

  // Calcule la distance en cm
  Distance = Duree * 0.034 / 2;

  if (Distance >= MesureMaxi || Distance <= MesureMini) {
    Serial.println("Distance de mesure en dehors de la plage (3 cm à 10 cm)");
    mainDetecte = 0;
  } else {
    mainDetecte = 1;
  }

  delay(1000);
  return mainDetecte;
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
  pinMode(TRIGPIN, OUTPUT); // Broche Trigger en sortie //
  pinMode(ECOPIN, INPUT); // Broche Echo en entree //



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
  switchState = digitalRead(PM_AM_SWITCH);

  if (millis() - previousMinuteEdit >= 20000) {
    updateLedMatrix(now);
  }

  detectionMain(); // Appel de la fonction de détection de main

  DateTime reveil = DateTime(now.year(), now.month(), now.day(), 20, 8, 0);

  unsigned long timerDuration = 100;
  unsigned long alarmDuration = 1000; // Durée de l'alarme allumée en millisecondes

  alarm(now, reveil, timerDuration, alarmDuration);
}
