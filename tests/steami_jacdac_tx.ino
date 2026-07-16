// Déclaration des LED de la STeaMi
#define LEDR PC12   // LED Rouge
#define LEDG PC11   // LED Verte
#define LEDB PC10   // LED Bleue

HardwareSerial JacdacSerial(PB7, PB6); 

void setup() {
  JacdacSerial.setHalfDuplex(); 
  JacdacSerial.begin(115200);

  // Configuration des broches des LEDs en sortie
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  
  // Éteindre toutes les LEDs au démarrage (selon le câblage, LOW ou HIGH éteint la LED)
  digitalWrite(LEDR, LOW);
  digitalWrite(LEDG, LOW);
  digitalWrite(LEDB, LOW);
}

void loop() {
  // 1. Allumer la LED Bleue pour signaler l'envoi
  digitalWrite(LEDB, HIGH);
  
  // 2. Envoyer le message
  JacdacSerial.println("V");
  
  // 3. Attendre 100 millisecondes (pour que l'éclair bleu soit bien visible)
  delay(100);
  
  // 4. Éteindre la LED Bleue
  digitalWrite(LEDB, LOW);
  
  // 5. Attendre 900 millisecondes (pour faire 1 seconde au total)
  delay(900);
}