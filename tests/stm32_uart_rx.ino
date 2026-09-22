// On déclare manuellement notre port série matériel sur les broches D0 (RX) et D1 (TX)
HardwareSerial camSerial(D0, D1); 

void setup() {
  // 1. Initialise la communication USB vers le PC
  Serial.begin(115200);
  
  // 2. Initialise la communication avec l'OpenMV
  camSerial.begin(115200);
  
  // 3. Configure la petite LED intégrée de la carte ST
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); // On l'éteint au démarrage
  
  // Laisse 2 secondes pour que tu aies le temps d'ouvrir le Moniteur Série
  delay(2000);
  Serial.println("--- STM32 prête ! En attente de l'OpenMV... ---");
}

void loop() {
  // Si un message arrive depuis la caméra
  if (camSerial.available()) {
    
    // On lit le message jusqu'à rencontrer le retour à la ligne (\n)
    String message = camSerial.readStringUntil('\n');
    
    // On nettoie le message (retire les espaces ou caractères invisibles)
    message.trim();
    
    // Si on a bien reçu au moins un caractère (n'importe quelle lettre)
    if (message.length() > 0) {
        
      // On affiche ce qu'on a reçu sur l'écran du PC
      Serial.println("Caméra dit : " + message);
      
      // On fait clignoter la LED très rapidement pour confirmer la réception
      digitalWrite(LED_BUILTIN, HIGH); 
      delay(50); // Pause très courte pour ne pas bloquer le flux de données                     
      digitalWrite(LED_BUILTIN, LOW);  
    }
  }
}