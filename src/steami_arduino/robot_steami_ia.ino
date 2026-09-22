/*
   Programme de mise en oeuvre du robot IA 4x4
   Robot équipé de :
   - carte microcontrôleur : STeaMi
   - servo moteur S90 (sonar)
   - capteur US HC-SR04 (sonar)
   - 4 moteurs CC avec codeurs commandés par 2 breakout L298
   - 6 TOF VL53L0X
   - bumper avec 2 switchs
   - stick 8LEDs Neopixel
   - communication BLE (https://apkpure.com/ble-simple-remote/com.ble.remote.simple/download)
*/
HardwareSerial JacdacSerial(PB7, PB6); //déclaration du serial jacdac avec la commande hardwareSerial qui le permet
#include <Servo.h>                // bibliothèque servo
#include <Wire.h>                 // bibliothèque pour liaison I2C
#include "PCF8574.h"              // bibliothèque pour GPIO expanders (https://github.com/RobTillaart/PCF8574)
#include <U8g2lib.h>              // bibliothèque pour afficheur OLED
#include <SPI.h>                  // bibliothèque SPI (afficheur OLED)
#include <VL53L0X.h>              // bibliothèque pour TOF
#include <Adafruit_NeoPixel.h>    // bibliothèque pour  Stick LED
#include <MCP23009E.h>            // bibliothèque pour GIOP expander STeaMi
#include "STM32TimerInterrupt.h"  // bibliothèque pour timer
#include "STM32_ISR_Timer.h"      // bibliothèque pour timer
#include <STM32duinoBLE.h>        // bibliothèque pour BLE
#include <ISM330DLCSensor.h>      // bibliothèque pour accéléromètre / gyroscope

#define DEBUG
#define Moteur_AVD
#define Moteur_ARD
#define Moteur_AVG
#define Moteur_ARG

// Broches du contrôleur I2C
#define I2C1_SDA PB9
#define I2C1_SCL PB8
#define I2C3_SDA PC1
#define I2C3_SCL PC0
// TOF VL53L0X
#define XTOF1 6  // vide droite
#define XTOF2 1  // vide gauche
#define XTOF3 7  // avant droite
#define XTOF4 0  // avant gauche
#define XTOF5 4  // arrière droite
#define XTOF6 3  // arrière gauche
// servo
#define servomotor PA7  // P5
// stick LED Néopixel
#define StickLED PA4  // P4
#define NbLED 8       // nombre de LEDs du stick
// US
#define trigUS 2    // broche trig du capteur US HC-SR04
#define echoUS PC3  // broche echo du capteur US HC-SR04
// Braekout L298 droite
#define IN1_ARD 4
#define IN2_ARD 5
#define ENA_ARD PA9  // P7 PWM
#define IN3_AVD 6
#define IN4_AVD 7
#define ENB_AVD PA5  // P1 PWM
// Braekout L298 gauche
#define IN1_AVG 0
#define IN2_AVG 1
#define ENA_AVG PA6  // P10 PWM
#define IN3_ARG 2
#define IN4_ARG 3
#define ENB_ARG PA8  // P11 PWM

// Codeurs moteurs
#define CO_AVD PC6   // P12
#define CO_ARD PC2   // P9
#define CO_AVG PB15  // P15
#define CO_ARG PB13  // P13

// Bumper
#define BMPD PC5  // P2
#define BMPG PC4  // P0

// BP Menu
#define BP_Menu PA0

// LEDs STeaMi
#define LEDR PC12   // LED RGB STeaMi Rouge
#define LEDG PC11   // LED RGB STeaMi Verte
#define LEDB PC10   // LED RGB STeaMi Bleue
#define LEDBLE PH3  // LED BLE

#define HW_TIMER_INTERVAL_MS 50

#define MAX_NB_CHAR (128)  // buffer BLE

// Correction TOF Robot N°004
#define CorrectionVideD 30
#define CorrectionVideG 40
#define CorrectionAvD 50
#define CorrectionAvG 40
#define CorrectionArrD 50
#define CorrectionArrG 35

const int RESET_PIN = RST_EXPANDER;

uint8_t posMenu = 0;
uint8_t lastUp = HIGH;
uint8_t lastDown = HIGH;
uint8_t lastLeft = HIGH;
uint8_t lastRight = HIGH;
uint8_t flagDown = false;   // Position up joystick dans le menu
uint8_t flagUp = false;     // Position down joystick dans le menu
uint8_t flagValid = false;  // Validation de la fonction dans le menu
uint8_t flagAffiche = false;
volatile uint8_t flagBMP = false;     // contact bumper (modifié par interruption)
uint8_t flagMenu = false;
volatile uint8_t flagArret = false;   // arrêt moteurs à effectuer (modifié par interruption)
volatile uint8_t flagBPMenu = false;  // appui BP Menu à traiter (modifié par interruption)
uint8_t flagInitBLE = false;          // Initialisation de la fonction BLE
uint8_t flagBLEconect = false;        // BLE est connecté
uint8_t flagLEDconnexion = false;     // LED bleue de connexion BLE à afficher
uint8_t angle = 0;                    // angle de rotation servo
uint8_t flagservo = false;            // sens de rotation servo

const uint16_t declTimerEch = 100;  // durée déclenchement timer (en ms)

volatile uint8_t fronts_CO_AVD = 0;  // Compteur de fronts du codeur
volatile uint8_t fronts_CO_ARD = 0;
volatile uint8_t fronts_CO_AVG = 0;
volatile uint8_t fronts_CO_ARG = 0;

volatile uint8_t compt_CO_AVD = 0;
volatile uint8_t compt_CO_ARD = 0;
volatile uint8_t compt_CO_ARG = 0;
volatile uint8_t compt_CO_AVG = 0;

int32_t accelerometer[3];
int32_t gyroscope[3];

unsigned long millisBLE = 0;
unsigned long millisSonar = 0;

Servo servomoteur;  // Nom du servo

TwoWire Wire3(I2C3_SDA, I2C3_SCL);  // Instanciations des contrôleurs I2C

U8G2_SSD1327_EA_W128128_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/PA1, /* data=*/PB5, /* cs=*/PD0, /* dc=*/PB4, /* reset=*/PA12);

Adafruit_NeoPixel strip(NbLED, StickLED, NEO_GRB + NEO_KHZ800);

PCF8574 pcf8574_0(0x38, &Wire3);  // (adresse IO exp., SDA, SCL) pour moteurs
PCF8574 pcf8574_1(0x39, &Wire3);  // (adresse IO exp., SDA, SCL) pour TOF + US

VL53L0X ToF1;
VL53L0X ToF2;
VL53L0X ToF3;
VL53L0X ToF4;
VL53L0X ToF5;
VL53L0X ToF6;

MCP23009E mcp(Wire, MCP23009_I2C_ADDR);

BLEService remoteService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");  // create service
BLEStringCharacteristic rxCharacteristic("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLEWrite, MAX_NB_CHAR);

STM32Timer ITimer(TIM1);  // Timer pour échantillonnage fourches optiques
STM32_ISR_Timer ISR_Timer;

TwoWire dev_i2c(I2C1_SDA, I2C1_SCL);
ISM330DLCSensor AccGyr(&dev_i2c);

// ********************* interruptions *****************************

void comptage_CO_AVD() {
  fronts_CO_AVD++;  // On incrémente le compteur de fronts du codeur
}

void comptage_CO_ARD() {
  fronts_CO_ARD++;  // On incrémente le compteur de fronts du codeur
}

void comptage_CO_AVG() {
  fronts_CO_AVG++;  // On incrémente le compteur de fronts du codeur
}

void comptage_CO_ARG() {
  fronts_CO_ARG++;  // On incrémente le compteur de fronts du codeur
}

void TimerHandler() {
  ISR_Timer.run();
}

// Les ISR ne font que signaler l'événement : le traitement (arrêt moteurs,
// liaison série, servo...) est effectué dans loop() par gestion_interruptions()
void contactBMPD() {
  flagBMP = true;
  flagArret = true;
}

void contactBMPG() {
  flagBMP = true;
  flagArret = true;
}

void BPMenu() {
  flagBPMenu = true;
}

/* Interruptions SimpleTimer */
void echant() {
  compt_CO_ARD = fronts_CO_ARD;
  compt_CO_ARG = fronts_CO_ARG;
  compt_CO_AVD = fronts_CO_AVD;
  compt_CO_AVG = fronts_CO_AVG;
  fronts_CO_ARD = 0;  // Ràz compteurs de fronts
  fronts_CO_ARG = 0;
  fronts_CO_AVD = 0;
  fronts_CO_AVG = 0;
}

// ********************* setup *************************************

void setup() {
  Serial.begin(9600);
  u8g2.begin();
  JacdacSerial.setHalfDuplex(); //activation de la com via le jacdac pour dire au code attention cette com fait les deux : RX et TX
  JacdacSerial.begin(115200); //et démarrage de la com à 115200 bauds.
  JacdacSerial.setTimeout(10); // timeout court (10 ms) : readStringUntil() ne bloque pas la boucle sur une trame incomplète
  affsetup();
  initialisation();
  stop();
  // Interruption sur contacts bumper
  attachInterrupt(digitalPinToInterrupt(BMPD), contactBMPD, FALLING);
  attachInterrupt(digitalPinToInterrupt(BMPG), contactBMPG, FALLING);
  // Interruption sur BP Menu
  attachInterrupt(digitalPinToInterrupt(BP_Menu), BPMenu, FALLING);
  // Interruptions sur fronts codeur
  attachInterrupt(digitalPinToInterrupt(CO_AVD), comptage_CO_AVD, FALLING);
  attachInterrupt(digitalPinToInterrupt(CO_ARD), comptage_CO_ARD, FALLING);
  attachInterrupt(digitalPinToInterrupt(CO_AVG), comptage_CO_AVG, FALLING);
  attachInterrupt(digitalPinToInterrupt(CO_ARG), comptage_CO_ARG, FALLING);
  if (ITimer.attachInterruptInterval(HW_TIMER_INTERVAL_MS * 1000, TimerHandler)) {
    Serial.print(F("Starting ITimer OK, millis() = "));
    Serial.println(millis());
  } else
    Serial.println(F("Can't set ITimer. Select another freq. or timer"));
  ISR_Timer.setInterval(declTimerEch, echant);
  flagDown = false;
  flagUp = false;
  flagValid = false;
  flagAffiche = false;
  flagBMP = false;
  flagMenu = false;
  flagArret = false;
  flagBPMenu = false;
  flagInitBLE = false;
  flagBLEconect = false;
  flagLEDconnexion = false;
  flagservo = false;
  angle = 90;
  servomoteur.attach(servomotor);  // on attache le servomoteur à sa broche
  servomoteur.write(angle);        // on positonne le servomoteur à 90 degrés
  delay(2000);
  servomoteur.detach();  // on détache le servomoteur
}

// ********************* loop *************************************

void loop() {
  gestion_interruptions();
  menu();
  gestion_IA();
}

// ********************* fonctions *************************************

//------- début de la fonction traitement des événements d'interruption -------
void gestion_interruptions() {
  if (flagArret) {  // contact bumper : arrêt des moteurs
    flagArret = false;
    stop();
  }
  if (flagBPMenu) {  // appui sur BP Menu : retour au menu principal
    flagBPMenu = false;
    stop();
    if (posMenu == 3) {  // si menu = sonar
#ifdef DEBUG
      Serial.println("Menu sonar");
#endif
      angle = 90;
      servomoteur.write(angle);  // on positonne le servomoteur à 90 degrés
      flagservo = false;
    }
    posMenu = 0;
    flagMenu = true;
    flagBMP = false;
    flagAffiche = false;
    flagValid = false;
  }
}
//------- fin de la fonction traitement des événements d'interruption -------

//-------------- début de la fonction affichage setup : ----------------
void affsetup() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tf);
  u8g2.drawStr(15, 50, "Robot IA 4x4");
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(25, 70, "GS Ingenierie");
  u8g2.sendBuffer();
#ifdef DEBUG
  Serial.println("Début setup");
#endif
}
//-------------- fin de la fonction affichage setup : --------------------

//------------------- début de la fonction initialisation ----------------
void initialisation() {
  uint8_t flagInit = true;
  Wire3.begin();  // Initialisation du bus I2C
  Wire.setSDA(I2C1_SDA);
  Wire.setSCL(I2C1_SCL);
  Wire.begin();
  dev_i2c.begin();
  AccGyr.begin();
  AccGyr.Enable_X();
  AccGyr.Enable_G();
  mcp.begin(RESET_PIN);
  mcp.setup(MCP23009_BTN_UP, MCP23009_DIR_INPUT, MCP23009_PULLUP);
  mcp.setup(MCP23009_BTN_DOWN, MCP23009_DIR_INPUT, MCP23009_PULLUP);
  mcp.setup(MCP23009_BTN_LEFT, MCP23009_DIR_INPUT, MCP23009_PULLUP);
  mcp.setup(MCP23009_BTN_RIGHT, MCP23009_DIR_INPUT, MCP23009_PULLUP);
  pinMode(ENA_ARD, OUTPUT);
  pinMode(ENB_AVD, OUTPUT);
  pinMode(ENA_AVG, OUTPUT);
  pinMode(ENB_ARG, OUTPUT);
  pinMode(CO_AVD, INPUT);
  pinMode(CO_ARD, INPUT);
  pinMode(CO_AVG, INPUT);
  pinMode(CO_ARG, INPUT);
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  pinMode(LEDBLE, OUTPUT);
  pinMode(echoUS, INPUT);        // la broche echo est initialisée en entree
  pcf8574_1.write(trigUS, LOW);  // (0V) sur la broche trig
  digitalWrite(LEDBLE, LOW);
  digitalWrite(LEDR, LOW);
  digitalWrite(LEDG, LOW);
  digitalWrite(LEDB, LOW);
  pinMode(BMPD, INPUT_PULLUP);
  pinMode(BMPG, INPUT_PULLUP);
  digitalWrite(ENB_AVD, LOW);
  digitalWrite(ENA_ARD, LOW);
  digitalWrite(ENA_AVG, LOW);
  digitalWrite(ENB_ARG, LOW);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(25, 10, "Initialisation");
  u8g2.setCursor(15, 25);
  u8g2.print("Init I/O Exp. 0 : ");
#ifdef DEBUG
  Serial.print("Init I/O Exp. 0 : ");
#endif
  if (pcf8574_0.begin()) {
    u8g2.print("OK");
#ifdef DEBUG
    Serial.println("OK");
#endif
  } else {
    u8g2.print("KO");
    flagInit = false;
#ifdef DEBUG
    Serial.println("KO");
#endif
  }
  u8g2.setCursor(15, 35);
  u8g2.print("Init I/O Exp. 1 : ");
#ifdef DEBUG
  Serial.print("Init I/O Exp. 1 : ");
#endif
  if (pcf8574_1.begin()) {
    u8g2.print("OK");
#ifdef DEBUG
    Serial.println("OK");
#endif
  } else {
    u8g2.print("KO");
    flagInit = false;
#ifdef DEBUG
    Serial.println("KO");
#endif
  }
  pcf8574_1.write(XTOF1, LOW);
  pcf8574_1.write(XTOF2, LOW);
  pcf8574_1.write(XTOF3, LOW);
  pcf8574_1.write(XTOF4, LOW);
  pcf8574_1.write(XTOF5, LOW);
  pcf8574_1.write(XTOF6, LOW);
  ToF1.setBus(&Wire3);
  ToF2.setBus(&Wire3);
  ToF3.setBus(&Wire3);
  ToF4.setBus(&Wire3);
  ToF5.setBus(&Wire3);
  ToF6.setBus(&Wire3);
  pcf8574_1.write(XTOF1, HIGH);
  ToF1.init(true);
  ToF1.setAddress((uint8_t)0x16);
  pcf8574_1.write(XTOF2, HIGH);
  ToF2.init(true);
  ToF2.setAddress((uint8_t)0x17);
  pcf8574_1.write(XTOF3, HIGH);
  ToF3.init(true);
  ToF3.setAddress((uint8_t)0x18);
  pcf8574_1.write(XTOF4, HIGH);
  ToF4.init(true);
  ToF4.setAddress((uint8_t)0x19);
  pcf8574_1.write(XTOF5, HIGH);
  ToF5.init(true);
  ToF5.setAddress((uint8_t)0x1A);
  pcf8574_1.write(XTOF6, HIGH);
  ToF6.init(true);
  ToF6.setAddress((uint8_t)0x1B);
  u8g2.setCursor(15, 45);
  u8g2.print("TOF1 : ");
#ifdef DEBUG
  Serial.print("Init TOF 1 : ");
#endif
  ToF1.setTimeout(500);
  if (ToF1.init()) {
    ToF1.startContinuous();
    u8g2.print("OK");
#ifdef DEBUG
    Serial.print("OK");
#endif
  } else {
    u8g2.print("KO");
    flagInit = false;
#ifdef DEBUG
    Serial.print("KO");
#endif
  }
  u8g2.print("    2 : ");
#ifdef DEBUG
  Serial.print("   |   Init TOF 2 : ");
#endif
  ToF2.setTimeout(500);
  if (ToF2.init()) {
    ToF2.startContinuous();
    u8g2.print("OK");
#ifdef DEBUG
    Serial.print("OK");
#endif
  } else {
    u8g2.print("KO");
    flagInit = false;
#ifdef DEBUG
    Serial.print("KO");
#endif
  }
  u8g2.setCursor(15, 55);
  u8g2.print("TOF3 : ");
#ifdef DEBUG
  Serial.print("   |   Init TOF 3 : ");
#endif
  ToF3.setTimeout(500);
  if (ToF3.init()) {
    ToF3.startContinuous();
    u8g2.print("OK");
#ifdef DEBUG
    Serial.print("OK");
#endif
  } else {
    u8g2.print("KO");
    flagInit = false;
#ifdef DEBUG
    Serial.print("KO");
#endif
  }
  u8g2.print("    4 : ");
#ifdef DEBUG
  Serial.print("   |   Init TOF 4 : ");
#endif
  ToF4.setTimeout(500);
  if (ToF4.init()) {
    ToF4.startContinuous();
    u8g2.print("OK");
#ifdef DEBUG
    Serial.print("OK");
#endif
  } else {
    u8g2.print("KO");
    flagInit = false;
#ifdef DEBUG
    Serial.print("KO");
#endif
  }
  u8g2.setCursor(15, 65);
  u8g2.print("TOF5 : ");
#ifdef DEBUG
  Serial.print("   |   Init TOF 5 : ");
#endif
  ToF5.setTimeout(500);
  if (ToF5.init()) {
    ToF5.startContinuous();
    u8g2.print("OK");
#ifdef DEBUG
    Serial.print("OK");
#endif
  } else {
    u8g2.print("KO");
    flagInit = false;
#ifdef DEBUG
    Serial.print("KO");
#endif
  }
  u8g2.print("    6 : ");
#ifdef DEBUG
  Serial.print("   |   Init TOF 6 : ");
#endif
  ToF6.setTimeout(500);
  if (ToF6.init()) {
    ToF6.startContinuous();
    u8g2.print("OK");
#ifdef DEBUG
    Serial.println("OK");
#endif
  } else {
    u8g2.print("KO");
    flagInit = false;
#ifdef DEBUG
    Serial.println("KO");
#endif
  }
  u8g2.sendBuffer();
  strip.begin();            // initialisation Stick Leds
  strip.show();             // on éteind toutes les LEDs
  strip.setBrightness(50);  // éclairement 20% (max = 255)
  rainbow(2);
  strip.clear();  // on éteind toutes les LEDs
  if (flagInit) {
    strip.setPixelColor(0, strip.Color(0, 150, 0));  // LEDs 1 = verte
    strip.setPixelColor(7, strip.Color(0, 150, 0));  // LEDs 8 = verte
    strip.show();                                    // affichage LEDs
  } else {
    strip.clear();                                   // on éteind toutes les LEDs
    strip.setPixelColor(0, strip.Color(150, 0, 0));  // LEDs 1 = rouge
    strip.setPixelColor(7, strip.Color(150, 0, 0));  // LEDs 8 = rouge
    strip.show();                                    // affichage LEDs
   //while (1) EN COMMENTAIRE POUR LE MOMENT POUR ENLEVER LE PB D4INIT
      ;
  }
}
//-------------- fin de la fonction initialisation -----------------

// ------- début de la fonction effet arc en ciel sur le stick de LEDs -----

void rainbow(int wait) {
  for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256) {
    for (int i = 0; i < strip.numPixels(); i++) {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();  // affichage LEDs
    delay(wait);   // pause
  }
}
// ------- fin de la fonction effet arc en ciel sur le stick de LEDs  : -----

// ----------- début de la fonction Menu ---------
void menu() {
  // #ifdef DEBUG
  //   Serial.print("position Menu = ");
  //   Serial.println(posMenu);
  // #endif
  uint8_t up = mcp.getLevel(MCP23009_BTN_UP);
  if (!up) flagUp = true;
  uint8_t down = mcp.getLevel(MCP23009_BTN_DOWN);
  if (!down) flagDown = true;
  uint8_t left = mcp.getLevel(MCP23009_BTN_LEFT);
  if (!left) flagValid = true;  // validation de la fonction dans le menu
  uint8_t right = mcp.getLevel(MCP23009_BTN_RIGHT);
  if (!right) flagValid = true;  // validation de la fonction dans le menu
  if (flagUp) {
    posMenu--;
    flagUp = false;
    flagAffiche = false;
  }
  if (flagDown) {
    posMenu++;
    flagDown = false;
    flagAffiche = false;
  }
  switch (posMenu) {
    case 0:
      if (flagValid) {
        if (!flagInitBLE) {
          Serial.println("Init BLE");
          initBLE();
          flagInitBLE = true;
        }
        BLE.poll();
        unsigned long nowMillis = millis();
        if (nowMillis - millisBLE > 4000) {  // tempo connexion BLE 4s
          if (flagBLEconect && flagLEDconnexion) digitalWrite(LEDB, HIGH);
        }
      }
      if (flagMenu) flagAffiche = false;
      if (!flagAffiche) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB10_tf);
        u8g2.setFontMode(1);
        u8g2.setDrawColor(1);                 // couleur box blanche
        u8g2.drawBox(13, 9, 35, 14);          // dessin de la box
        u8g2.setDrawColor(0);                 // couleur texte noir
        u8g2.drawStr(15, 22, "BLE");          // texte
        u8g2.setDrawColor(1);                 // couleur texte blanc
        u8g2.drawStr(15, 39, "Deplacement");  // texte
        u8g2.drawStr(15, 56, "TOF");          // texte
        u8g2.drawStr(15, 73, "Sonar");        // texte
        u8g2.drawStr(15, 90, "IMU");          // texte
        u8g2.sendBuffer();
        flagAffiche = true;
        flagMenu = false;
      }
      break;
    case 1:
      if (flagValid) {
        deplacement();
#ifdef DEBUG
        Serial.println("deplacement");
#endif
      }
      if (!flagAffiche) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB10_tf);
        u8g2.setFontMode(1);
        u8g2.setDrawColor(1);  // couleur box blanche
        u8g2.drawStr(15, 22, "BLE");
        u8g2.drawBox(13, 27, 106, 16);  // dessin de la box
        u8g2.setDrawColor(0);           // couleur texte noir
        u8g2.drawStr(15, 39, "Deplacement");
        u8g2.setDrawColor(1);           // couleur texte blanc
        u8g2.drawStr(15, 56, "TOF");    // texte
        u8g2.drawStr(15, 73, "Sonar");  // texte
        u8g2.drawStr(15, 90, "IMU");    // texte
        u8g2.sendBuffer();
        flagAffiche = true;
      }
      break;
    case 2:
      if (flagValid) {
        mesureTOF();
#ifdef DEBUG
        Serial.println("TOF");
#endif
      }
      if (!flagAffiche) {
        u8g2.clearBuffer();
        u8g2.setFontMode(1);
        u8g2.setDrawColor(1);  // couleur box blanche
        u8g2.drawStr(15, 22, "BLE");
        u8g2.drawStr(15, 39, "Deplacement");
        u8g2.drawBox(13, 44, 35, 14);   // dessin de la box
        u8g2.setDrawColor(0);           // couleur texte noir
        u8g2.drawStr(15, 56, "TOF");    // texte
        u8g2.setDrawColor(1);           // couleur texte blanc
        u8g2.drawStr(15, 73, "Sonar");  // texte
        u8g2.drawStr(15, 90, "IMU");    // texte
        u8g2.sendBuffer();
        flagAffiche = true;
      }
      break;
    case 3:
      if (flagValid) {
#ifdef DEBUG
        Serial.println("sonar");
#endif
        sonar();
      }
      if (!flagAffiche) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB10_tf);
        u8g2.setFontMode(1);
        u8g2.setDrawColor(1);  // couleur texte et box blanc
        u8g2.drawStr(15, 22, "BLE");
        u8g2.drawStr(15, 39, "Deplacement");
        u8g2.drawStr(15, 56, "TOF");    // texte
        u8g2.drawBox(13, 61, 50, 14);   // dessin de la box
        u8g2.setDrawColor(0);           // couleur texte noir
        u8g2.drawStr(15, 73, "Sonar");  // texte
        u8g2.setDrawColor(1);           // couleur texte blanc
        u8g2.drawStr(15, 90, "IMU");    // texte
        u8g2.sendBuffer();
        flagAffiche = true;
      }
      break;
    case 4:
      if (flagValid) {
        IMU();
#ifdef DEBUG
        Serial.println("IMU");
#endif
      }
      if (!flagAffiche) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB10_tf);
        u8g2.setFontMode(1);
        u8g2.setDrawColor(1);  // couleur texte et box blanc
        u8g2.drawStr(15, 22, "BLE");
        u8g2.drawStr(15, 39, "Deplacement");
        u8g2.drawStr(15, 56, "TOF");    // texte
        u8g2.drawStr(15, 73, "Sonar");  // texte
        u8g2.drawBox(13, 78, 35, 14);   // dessin de la box
        u8g2.setDrawColor(0);           // couleur texte noir
        u8g2.drawStr(15, 90, "IMU");    // texte
        u8g2.setDrawColor(1);           // couleur texte blanc
        u8g2.sendBuffer();
        flagAffiche = true;
      }
      break;
    default:
      if (posMenu == 5) {
        posMenu = 0;
        flagAffiche = false;
      }
      if (posMenu == 255) {
        posMenu = 4;
        flagAffiche = false;
      }
      break;
  }
  delay(150);
}
// ----------- fin de la fonction Menu ---------

//--------------- début de la fonction deplacement : ----------------
void deplacement() {
  if (!flagBMP) avant(150);
  affCO();
  // unsigned long nowMillis = millis();
  // if (nowMillis - millisAvant > 20000) {  // tempo 10s
  //   stop();
  //   delay(1000);
  //   arriere(100);
  //   delay(3000);
  //   stop();
  //   delay(1000);
  //   millisAvant = millis();
  // }
}
//--------------- fin de la fonction deplacement : ----------------

//------------------- début de la fonction marche avant : -----------------
void avant(uint8_t vitesse) {
  Serial.println("Marche avant");
#ifdef Moteur_AVD
  pcf8574_0.write(IN3_AVD, LOW);
  pcf8574_0.write(IN4_AVD, HIGH);
  analogWrite(ENB_AVD, vitesse);
#endif
#ifdef Moteur_ARD
  pcf8574_0.write(IN1_ARD, LOW);
  pcf8574_0.write(IN2_ARD, HIGH);
  analogWrite(ENA_ARD, vitesse);
#endif
#ifdef Moteur_AVG
  pcf8574_0.write(IN1_AVG, LOW);
  pcf8574_0.write(IN2_AVG, HIGH);
  analogWrite(ENA_AVG, vitesse);
#endif
#ifdef Moteur_ARG
  pcf8574_0.write(IN3_ARG, LOW);
  pcf8574_0.write(IN4_ARG, HIGH);
  analogWrite(ENB_ARG, vitesse);
#endif
}
//------------------- fin de la fonction marche avant : -----------------

//------------------- début de la fonction marche arriere : -----------------
void arriere(uint8_t vitesse) {
  Serial.println("Marche arrière");
#ifdef Moteur_AVD
  pcf8574_0.write(IN3_AVD, HIGH);
  pcf8574_0.write(IN4_AVD, LOW);
  analogWrite(ENB_AVD, vitesse);
#endif
#ifdef Moteur_ARD
  pcf8574_0.write(IN1_ARD, HIGH);
  pcf8574_0.write(IN2_ARD, LOW);
  analogWrite(ENA_ARD, vitesse);
#endif
#ifdef Moteur_AVG
  pcf8574_0.write(IN1_AVG, HIGH);
  pcf8574_0.write(IN2_AVG, LOW);
  analogWrite(ENA_AVG, vitesse);
#endif
#ifdef Moteur_ARG
  pcf8574_0.write(IN3_ARG, HIGH);
  pcf8574_0.write(IN4_ARG, LOW);
  analogWrite(ENB_ARG, vitesse);
#endif
}
//------------------- fin de la fonction marche arriere : -----------------

//---------- debut des fonctions sens des déplacement --------------
void trans_gauche(byte vitesse) {
  pcf8574_0.write(IN3_AVD, LOW);  // sens avant
  pcf8574_0.write(IN4_AVD, HIGH);
  analogWrite(ENB_AVD, vitesse);
  pcf8574_0.write(IN1_ARD, HIGH);  // sens arriere
  pcf8574_0.write(IN2_ARD, LOW);
  analogWrite(ENA_ARD, vitesse);
  pcf8574_0.write(IN3_ARG, LOW);  // sens avant
  pcf8574_0.write(IN4_ARG, HIGH);
  analogWrite(ENB_ARG, vitesse);
  pcf8574_0.write(IN1_AVG, HIGH);  // sens arriere
  pcf8574_0.write(IN2_AVG, LOW);
  analogWrite(ENA_AVG, vitesse);
}

void trans_droite(byte vitesse) {
  pcf8574_0.write(IN3_AVD, HIGH);  // sens arriere
  pcf8574_0.write(IN4_AVD, LOW);
  analogWrite(ENB_AVD, vitesse);
  pcf8574_0.write(IN1_ARD, LOW);  // sens avant
  pcf8574_0.write(IN2_ARD, HIGH);
  analogWrite(ENA_ARD, vitesse);
  pcf8574_0.write(IN3_ARG, HIGH);  // sens arriere
  pcf8574_0.write(IN4_ARG, LOW);
  analogWrite(ENB_ARG, vitesse);
  pcf8574_0.write(IN1_AVG, LOW);  // sens avant
  pcf8574_0.write(IN2_AVG, HIGH);
  analogWrite(ENA_AVG, vitesse);
}

void rot_droite(byte vitesse) {
  pcf8574_0.write(IN3_AVD, HIGH);  // sens arriere
  pcf8574_0.write(IN4_AVD, LOW);
  analogWrite(ENB_AVD, vitesse);
  pcf8574_0.write(IN1_ARD, HIGH);  // sens arriere
  pcf8574_0.write(IN2_ARD, LOW);
  analogWrite(ENA_ARD, vitesse);
  pcf8574_0.write(IN3_ARG, LOW);  // sens avant
  pcf8574_0.write(IN4_ARG, HIGH);
  analogWrite(ENB_ARG, vitesse);
  pcf8574_0.write(IN1_AVG, LOW);  // sens avant
  pcf8574_0.write(IN2_AVG, HIGH);
  analogWrite(ENA_AVG, vitesse);
}

void rot_gauche(byte vitesse) {
  pcf8574_0.write(IN3_AVD, LOW);  // sens avant
  pcf8574_0.write(IN4_AVD, HIGH);
  analogWrite(ENB_AVD, vitesse);
  pcf8574_0.write(IN1_ARD, LOW);  // sens avant
  pcf8574_0.write(IN2_ARD, HIGH);
  analogWrite(ENA_ARD, vitesse);
  pcf8574_0.write(IN3_ARG, HIGH);  // sens arriere
  pcf8574_0.write(IN4_ARG, LOW);
  analogWrite(ENB_ARG, vitesse);
  pcf8574_0.write(IN1_AVG, HIGH);  // sens arriere
  pcf8574_0.write(IN2_AVG, LOW);
  analogWrite(ENA_AVG, vitesse);
}

void diagav_gauche(byte vitesse) {
  pcf8574_0.write(IN3_AVD, LOW);  // sens avant
  pcf8574_0.write(IN4_AVD, HIGH);
  analogWrite(ENB_AVD, vitesse);
  pcf8574_0.write(IN1_ARD, LOW);
  pcf8574_0.write(IN2_ARD, LOW);
  digitalWrite(ENA_ARD, LOW);
  pcf8574_0.write(IN3_ARG, LOW);  // sens avant
  pcf8574_0.write(IN4_ARG, HIGH);
  analogWrite(ENB_ARG, vitesse);
  pcf8574_0.write(IN1_AVG, LOW);
  pcf8574_0.write(IN2_AVG, LOW);
  digitalWrite(ENA_AVG, LOW);
}

void diagav_droite(byte vitesse) {
  pcf8574_0.write(IN3_AVD, LOW);
  pcf8574_0.write(IN4_AVD, LOW);
  digitalWrite(ENB_AVD, LOW);
  pcf8574_0.write(IN1_ARD, LOW);  // sens avant
  pcf8574_0.write(IN2_ARD, HIGH);
  analogWrite(ENA_ARD, vitesse);
  pcf8574_0.write(IN3_ARG, LOW);
  pcf8574_0.write(IN4_ARG, LOW);
  digitalWrite(ENB_ARG, LOW);
  pcf8574_0.write(IN1_AVG, LOW);  // sens avant
  pcf8574_0.write(IN2_AVG, HIGH);
  analogWrite(ENA_AVG, vitesse);
}

void diagar_droite(byte vitesse) {
  pcf8574_0.write(IN3_AVD, HIGH);  // sens arriere
  pcf8574_0.write(IN4_AVD, LOW);
  analogWrite(ENB_AVD, vitesse);
  pcf8574_0.write(IN1_ARD, LOW);
  pcf8574_0.write(IN2_ARD, LOW);
  digitalWrite(ENA_ARD, LOW);
  pcf8574_0.write(IN3_ARG, HIGH);  // sens arriere
  pcf8574_0.write(IN4_ARG, LOW);
  analogWrite(ENB_ARG, vitesse);
  pcf8574_0.write(IN1_AVG, LOW);
  pcf8574_0.write(IN2_AVG, LOW);
  digitalWrite(ENA_AVG, LOW);
}

void diagar_gauche(byte vitesse) {
  pcf8574_0.write(IN3_AVD, LOW);
  pcf8574_0.write(IN4_AVD, LOW);
  digitalWrite(ENB_AVD, LOW);
  pcf8574_0.write(IN1_ARD, HIGH);  // sens arriere
  pcf8574_0.write(IN2_ARD, LOW);
  analogWrite(ENA_ARD, vitesse);
  pcf8574_0.write(IN3_ARG, LOW);
  pcf8574_0.write(IN4_ARG, LOW);
  digitalWrite(ENB_ARG, LOW);
  pcf8574_0.write(IN1_AVG, HIGH);  // sens arriere
  pcf8574_0.write(IN2_AVG, LOW);
  analogWrite(ENA_AVG, vitesse);
}
//---------- fin des fonctions sens des déplacement --------------

//------------------- début de la fonction stop : -----------------
void stop() {
  Serial.println("Stop");
#ifdef Moteur_AVD
  pcf8574_0.write(IN3_AVD, LOW);
  pcf8574_0.write(IN4_AVD, LOW);
  digitalWrite(ENB_AVD, LOW);
#endif
#ifdef Moteur_ARD
  pcf8574_0.write(IN1_ARD, LOW);
  pcf8574_0.write(IN2_ARD, LOW);
  digitalWrite(ENA_ARD, LOW);
#endif
#ifdef Moteur_AVG
  pcf8574_0.write(IN1_AVG, LOW);
  pcf8574_0.write(IN2_AVG, LOW);
  digitalWrite(ENA_AVG, LOW);
#endif
#ifdef Moteur_ARG
  pcf8574_0.write(IN3_ARG, LOW);
  pcf8574_0.write(IN4_ARG, LOW);
  digitalWrite(ENB_ARG, LOW);
#endif
}
//------------------- fin de la fonction stop : -----------------

//-------------- début de la fonction affichage codeurs  : ---------
void affCO() {
  Serial.print("Codeur AVD : ");
  Serial.print(compt_CO_AVD);
  Serial.print("   |   Codeur ARD : ");
  Serial.print(compt_CO_ARD);
  Serial.print("   |   Codeur AVG : ");
  Serial.print(compt_CO_AVG);
  Serial.print("   |   Codeur ARG : ");
  Serial.println(compt_CO_ARG);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.setCursor(15, 20);
  u8g2.print("CO AVD = ");
  u8g2.print(compt_CO_AVD);
  u8g2.print("   ");
  u8g2.setCursor(15, 30);
  u8g2.print("CO ARD = ");
  u8g2.print(compt_CO_ARD);
  u8g2.print("   ");
  u8g2.setCursor(15, 40);
  u8g2.print("CO AVG = ");
  u8g2.print(compt_CO_AVG);
  u8g2.print("   ");
  u8g2.setCursor(15, 50);
  u8g2.print("CO ARG = ");
  u8g2.print(compt_CO_ARG);
  u8g2.print("   ");
  u8g2.sendBuffer();
}
//--------------- fin de la fonction affichage codeurs  : ---------

//--------------- début de la fonction mesure TOFs : ----------------
void mesureTOF() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(10, 30);
  uint16_t mesTOF1 = ((ToF1.readRangeContinuousMillimeters()) - CorrectionVideD);
  u8g2.print("TOF Vide D:         ");
  u8g2.setCursor(80, 30);
  u8g2.print(mesTOF1);
  u8g2.println(" mm   ");
#ifdef DEBUG
  Serial.print("TOF Vide D = ");
  Serial.print(mesTOF1);
  Serial.print(" mm");
#endif
  if (ToF1.timeoutOccurred()) {
    u8g2.println(" TIMEOUT");
#ifdef DEBUG
    Serial.println(" TIMEOUT");
#endif
  }
  u8g2.setCursor(10, 40);
  uint16_t mesTOF2 = ((ToF2.readRangeContinuousMillimeters()) - CorrectionVideG);
  u8g2.print("TOF Vide G:          ");
  u8g2.setCursor(80, 40);
  u8g2.print(mesTOF2);
  u8g2.println(" mm   ");
#ifdef DEBUG
  Serial.print("   |   TOF Vide G = ");
  Serial.print(mesTOF2);
  Serial.print(" mm");
#endif
  if (ToF2.timeoutOccurred()) {
    u8g2.println(" TIMEOUT");
#ifdef DEBUG
    Serial.println(" TIMEOUT");
#endif
  }
  u8g2.setCursor(10, 50);
  uint16_t mesTOF3 = ((ToF3.readRangeContinuousMillimeters()) - CorrectionAvD);
  u8g2.print("TOF Av.   D :        ");
  u8g2.setCursor(80, 50);
  if (mesTOF3 > 2000) {
    u8g2.print(">2000");
  } else u8g2.print(mesTOF3);
  u8g2.println(" mm   ");
#ifdef DEBUG
  Serial.print("   |   TOF Av.  D = ");
  Serial.print(mesTOF3);
  Serial.print(" mm");
#endif
  if (ToF3.timeoutOccurred()) {
    u8g2.println(" TIMEOUT");
#ifdef DEBUG
    Serial.println(" TIMEOUT");
#endif
  }
  u8g2.setCursor(10, 60);
  uint16_t mesTOF4 = ((ToF4.readRangeContinuousMillimeters()) - CorrectionAvG);
  u8g2.print("TOF Av.   G :         ");
  u8g2.setCursor(80, 60);
  if (mesTOF4 > 2000) {
    u8g2.print(">2000");
  } else u8g2.print(mesTOF4);
  u8g2.println(" mm   ");
#ifdef DEBUG
  Serial.print("   |   TOF Av.  G = ");
  Serial.print(mesTOF4);
  Serial.print(" mm");
#endif
  if (ToF4.timeoutOccurred()) {
    u8g2.println(" TIMEOUT");
#ifdef DEBUG
    Serial.println(" TIMEOUT");
#endif
  }
  u8g2.setCursor(10, 70);
  uint16_t mesTOF5 = ((ToF5.readRangeContinuousMillimeters()) - CorrectionArrD);
  u8g2.print("TOF Arr   D :        ");
  u8g2.setCursor(80, 70);
  if (mesTOF5 > 2000) {
    u8g2.print(">2000");
  } else u8g2.print(mesTOF5);
  u8g2.println(" mm   ");
#ifdef DEBUG
  Serial.print("   |   TOF Arr  D = ");
  Serial.print(mesTOF5);
  Serial.print(" mm");
#endif
  if (ToF5.timeoutOccurred()) {
    u8g2.println(" TIMEOUT");
#ifdef DEBUG
    Serial.println(" TIMEOUT");
#endif
  }
  u8g2.setCursor(10, 80);
  uint16_t mesTOF6 = ((ToF6.readRangeContinuousMillimeters()) - CorrectionArrG);
  u8g2.print("TOF Arr   G :         ");
  u8g2.setCursor(80, 80);
  if (mesTOF6 > 2000) {
    u8g2.print(">2000");
  } else u8g2.print(mesTOF6);
  u8g2.println(" mm   ");
#ifdef DEBUG
  Serial.print("   |   TOF Arr  G = ");
  Serial.print(mesTOF6);
  Serial.println(" mm");
#endif
  if (ToF6.timeoutOccurred()) {
    u8g2.println(" TIMEOUT");
#ifdef DEBUG
    Serial.println(" TIMEOUT");
#endif
  }
  delay(200);
  u8g2.sendBuffer();
}
//------------------ fin de la fonction mesure TOFs : ---------------------

//--------------- début de la fonction sonar : ----------------
void sonar() {
  servomoteur.attach(servomotor);
  unsigned long nowMillis = millis();
  if (nowMillis - millisSonar > 500) {  // mesure toutes les 0.5s
    millisSonar = millis();
    mesureUS(angle);
    if (!flagservo) {
      if (angle < 180) angle = angle + 10;
      if (angle == 180) flagservo = true;
    } else {
      if (angle > 0) angle = angle - 10;
      if (angle == 0) flagservo = false;
    }
  }
}

void mesureUS(uint8_t angle) {
  servomoteur.write(angle);
  pcf8574_1.write(trigUS, HIGH);              // met 1 sur la broche trig
  delayMicroseconds(10);                      // attente pendant 10 µs
  pcf8574_1.write(trigUS, LOW);               // met 0 sur la broche trig.
  long lecture_echo = pulseIn(echoUS, HIGH);  // durée du 1 sur  echo (en µs)
  long distance = lecture_echo * 0.034 / 2;   // C = 0.034 cm/μs (AR => /2)
#ifdef DEBUG
  Serial.print("Angle = ");
  Serial.print(angle);
  Serial.print("°   ");
  Serial.print("Mesure US = ");
  Serial.print(lecture_echo);
  Serial.print("µs => ");
  Serial.print(distance);
  Serial.println("cm");
#endif
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.setCursor(15, 20);
  u8g2.print("Sonar");
  u8g2.setCursor(15, 40);
  u8g2.print("Angle = ");
  u8g2.print(angle);
  u8g2.print(" deg.");
  u8g2.setCursor(15, 50);
  u8g2.print("US = ");
  u8g2.print(distance);
  u8g2.print(" cm");
  u8g2.sendBuffer();
}

//--------------- fin de la fonction sonar : ----------------

//--------------- début de la fonction IMU : ----------------
void IMU() {
#ifdef DEBUG
  Serial.println("IMU");
#endif
  AccGyr.Get_X_Axes(accelerometer);
  AccGyr.Get_G_Axes(gyroscope);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.setCursor(15, 10);
  u8g2.print("Acc(x) = ");
  u8g2.print(accelerometer[0]);
  u8g2.print("       ");
  u8g2.setCursor(15, 20);
  u8g2.print("Acc(y) = ");
  u8g2.print(accelerometer[1]);
  u8g2.print("       ");
  u8g2.setCursor(15, 30);
  u8g2.print("Acc(z) = ");
  u8g2.print(accelerometer[2]);
  u8g2.print("       ");
  u8g2.setCursor(15, 45);
  u8g2.print("Gyr(x) = ");
  u8g2.print(gyroscope[0]);
  u8g2.print("       ");
  u8g2.setCursor(15, 55);
  u8g2.print("Gyr(y) = ");
  u8g2.print(gyroscope[1]);
  u8g2.print("       ");
  u8g2.setCursor(15, 65);
  u8g2.print("Gyr(z) = ");
  u8g2.print(gyroscope[2]);
  u8g2.print("       ");
  u8g2.sendBuffer();
#ifdef DEBUG
  Serial.print("Acc(x) = ");
  Serial.print(accelerometer[0]);
  Serial.print("  |  Acc(y) = ");
  Serial.print(accelerometer[1]);
  Serial.print("  |  Acc(z) = ");
  Serial.println(accelerometer[2]);
  Serial.print("Gyr(x) = ");
  Serial.print(gyroscope[0]);
  Serial.print("  |  Gyr(y) = ");
  Serial.print(gyroscope[1]);
  Serial.print("  |  Gyr(z) = ");
  Serial.println(gyroscope[2]);
#endif
}
//--------------- fin de la fonction IMU : ----------------


//----------------------- début des fonctions BLE  : ---------------------

void initBLE() {
  u8g2.setCursor(15, 70);
  u8g2.print("BLE   : ");
  if (BLE.begin()) {
    u8g2.print("OK");
    BLE.setLocalName("Robot_IA_4x4");
    BLE.setAdvertisedService(remoteService);
    remoteService.addCharacteristic(rxCharacteristic);
    BLE.addService(remoteService);
    BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
    BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
    rxCharacteristic.setEventHandler(BLEWritten, rxCharacteristicWritten);
    rxCharacteristic.setValue("");
    BLE.advertise();
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setDrawColor(1);  // couleur textes blancs
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(50, 40, "BLE");
    u8g2.drawStr(35, 60, "Attente");
    u8g2.drawStr(25, 80, "connexion");
    u8g2.sendBuffer();
  } else {
    u8g2.print("KO");
  }
}

void blePeripheralConnectHandler(BLEDevice central) {
  // central connected event handler
  digitalWrite(LEDBLE, HIGH);
  millisBLE = millis();
  flagBLEconect = true;
  flagLEDconnexion = true;
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setDrawColor(1);  // couleur textes blancs
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(50, 50, "BLE");
  u8g2.drawStr(10, 70, "Connexion OK");
  u8g2.sendBuffer();
#ifdef DEBUG
  Serial.print("Connected event, central MAC : ");
  Serial.println(central.address());
#endif
}

void blePeripheralDisconnectHandler(BLEDevice central) {
  // central disconnected event handler
  digitalWrite(LEDBLE, LOW);
  digitalWrite(LEDB, LOW);
  flagBLEconect = false;
  flagLEDconnexion = false;
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setDrawColor(1);  // couleur textes blancs
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(50, 40, "BLE");
  u8g2.drawStr(35, 60, "Attente");
  u8g2.drawStr(25, 80, "connexion");
  u8g2.sendBuffer();
#ifdef DEBUG
  Serial.print("Disconnected event, central MAC : ");
  Serial.println(central.address());
#endif
}

void rxCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // central wrote new value to characteristic
  // trame attendue : "[canal:]valeur1,valeur2,"
  int debut = 0;
  char canal = '0';
#ifdef DEBUG
  Serial.print("Characteristic event, written :   ");
#endif
  String reception = rxCharacteristic.value();
  if (reception.length() >= 2 && reception[1] == ':') {
#ifdef DEBUG
    Serial.print("canal : ");
    Serial.print(reception[0]);
    Serial.print("  |   ");
#endif
    canal = reception[0];
    debut = 2;
  }
  int virgule1 = reception.indexOf(',', debut);
  int virgule2 = (virgule1 < 0) ? -1 : reception.indexOf(',', virgule1 + 1);
  if (virgule2 < 0) {  // trame malformée (séparateur manquant) : ignorée
#ifdef DEBUG
    Serial.println("trame invalide");
#endif
    return;
  }
  String rec1 = reception.substring(debut, virgule1 + 1);         // valeur 1 (avec la virgule)
  String rec2 = reception.substring(virgule1 + 1, virgule2 + 1);  // valeur 2 (avec la virgule)
  int valrec2 = rec2.toInt();  // conversion de la chaîne de carctres en entier
  int valrec1 = rec1.toInt();
  int valrec = abs(valrec2);  // valeur absolue
  int valPwm = map(valrec, 0, 99, 0, 255);
#ifdef DEBUG
  Serial.print("rec1 = (");
  Serial.print(rec1);
  Serial.print(")");
  Serial.print("   |   rec2 = (");
  Serial.print(rec2);
  Serial.print(")  = ");
  Serial.print(valrec2);
#endif
  if (rec1 == "0," && rec2 == "0,") {  // commande stop
    stop();
    digitalWrite(LEDR, LOW);
    digitalWrite(LEDG, LOW);
    digitalWrite(LEDB, LOW);
    flagLEDconnexion = false;
#ifdef DEBUG
    Serial.println("  =>  LED Stop");
#endif
  } else {
    if (rec1 == "0," && (valrec2 > 0)) {  // commande marche avant
      avant(valPwm);
      digitalWrite(LEDG, HIGH);
      digitalWrite(LEDR, LOW);
      digitalWrite(LEDB, LOW);
      flagLEDconnexion = false;
#ifdef DEBUG
      Serial.println("  =>  LED Verte");
#endif
    }
    if (rec1 == "0," && (valrec2 < 0)) {  // commande marche arrière
      arriere(valPwm);
      digitalWrite(LEDR, HIGH);
      digitalWrite(LEDG, LOW);
      digitalWrite(LEDB, LOW);
      flagLEDconnexion = false;
#ifdef DEBUG
      Serial.println("  =>  LED Rouge");
#endif
    }
    if (-valrec1 == valrec2) {  // commande gauche
      digitalWrite(LEDB, LOW);
      flagLEDconnexion = false;
      switch (canal) {
        case '0':
          rot_gauche(valPwm);
          break;
        case '1':
          trans_gauche(valPwm);
          break;
        case '2':
          diagav_gauche(valPwm);
          break;
        case '3':
          diagar_gauche(valPwm);
          break;
      }
#ifdef DEBUG
      Serial.println("  =>  Gauche");
#endif
    }
    if (valrec1 == valrec2) {  // commande droite
      digitalWrite(LEDB, LOW);
      flagLEDconnexion = false;
      switch (canal) {
        case '0':
          rot_droite(valPwm);
          break;
        case '1':
          trans_droite(valPwm);
          break;
        case '2':
          diagav_droite(valPwm);
          break;
        case '3':
          diagar_droite(valPwm);
          break;
      }
#ifdef DEBUG
      Serial.println("  =>  Droite");
#endif
    }
  }
}
//----------------------- fin des fonctions BLE  ---------------------

//------------------- début de la fonction gestion IA : -----------------

void gestion_IA() {
  // 0. Sécurité anti-crash : Si le robot tape un mur (bumper), on bloque tout
  // même si la caméra dit d'avancer.
  if (flagBMP) {
    stop();
    return; 
  }

  // On écoute le câble relié à la caméra OpenMV
  if (JacdacSerial.available()) {
    
    // On lit la prédiction envoyée par MicroPython (la cam openMV)
    String message = JacdacSerial.readStringUntil('\n');
    message.trim(); 
    
    // Si on a bien reçu quelque chose
    if (message.length() > 0) {
      
      // 1. On éteint les LEDs par défaut avant d'appliquer le nouvel ordre
      digitalWrite(LEDR, LOW);
      digitalWrite(LEDG, LOW);
      digitalWrite(LEDB, LOW);
      strip.clear(); 
      
      // 2. LOGIQUE D'ACTION (MOTEURS + LUMIÈRES)
      if (message == "S") {
        // --- PANNEAU STOP ---
        stop();                               // Le robot s'arrête net
        digitalWrite(LEDR, HIGH);             // LED STeaMi Rouge
        strip.fill(strip.Color(150, 0, 0));   // Bandeau Robot Rouge
      } 
      else if (message == "V") {
        // --- PANNEAU 30 KM/H ---
        avant(120);                           // Le robot avance (Vitesse modérée 120 sur 255)
        digitalWrite(LEDG, HIGH);             // LED STeaMi Verte
        strip.fill(strip.Color(0, 150, 0));   // Bandeau Robot Vert
      } 
      else if (message == "I") {
        // --- PANNEAU SENS INTERDIT ---
        arriere(120);                         // Le robot recule 
        digitalWrite(LEDB, HIGH);             // LED STeaMi Bleue
        strip.fill(strip.Color(0, 0, 150));   // Bandeau Robot Bleu
      }
      else if (message == "N") {
        // --- AUCUN PANNEAU (Mode Normal) ---
        avant(150);                           // Le robot roule à sa vitesse de croisière (150)
        // Les LEDs restent éteintes.
      }

      // 3. On affiche la lumière
      strip.show();
    }
  }
}
//------------------- fin de la fonction gestion IA : -----------------