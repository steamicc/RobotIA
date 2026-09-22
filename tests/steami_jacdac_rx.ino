#define LEDR PC12   // LED Rouge
#define LEDG PC11   // LED Verte
#define LEDB PC10   // LED Bleue

HardwareSerial JacdacSerial(PB7, PB6); 

void setup() {
  // On garde la connexion PC pour voir le texte
  Serial.begin(115200);
  
  JacdacSerial.setHalfDuplex();
  JacdacSerial.begin(115200);
  
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  
  digitalWrite(LEDR, LOW);
  digitalWrite(LEDG, LOW);
  digitalWrite(LEDB, LOW);

  delay(2000);
  Serial.println("--- STeaMi 2 : RÉCEPTEUR PRÊT ---");
}

void loop() {
  // Dès qu'un signal arrive sur le câble Jacdac...
  if (JacdacSerial.available()) {
    
    // On lit le message
    String message = JacdacSerial.readStringUntil('\n');
    message.trim(); 
    
    // On l'affiche sur l'ordinateur
    Serial.println("Message reçu : " + message);

    // Retour visuel : Flash de la LED Rouge !
    digitalWrite(LEDR, HIGH); // Allume la LED
    delay(100);               // Laisse la LED allumée un court instant
    digitalWrite(LEDR, LOW);  // Éteint la LED
  }
}