# Tests de Communication et Portabilité Matérielle

Ce dossier contient des codes de validation (Proof of Concept) permettant de tester les protocoles de communication série utilisés dans le projet. Ils servent d'outils de diagnostic matériel et démontrent la portabilité du système vers d'autres plateformes.

## 1. Test de communication Jacdac (STeaMi vers STeaMi)

Ces scripts permettent de s'assurer que le port Jacdac des cartes STeaMi fonctionne correctement pour transmettre des données série brutes, en utilisant la fonction `setHalfDuplex()`.

* **Fichiers :** 
  * `steami_jacdac_tx.ino` (Carte émettrice)
  * `steami_jacdac_rx.ino` (Carte réceptrice)
* **Câblage :** Relier directement le port Jacdac de la carte A au port Jacdac de la carte B à l'aide d'un câble standard (GND, DATA, VCC).
* **Usage :** Flasher les deux cartes et ouvrir le moniteur série de la carte réceptrice (115200 bauds) pour vérifier la bonne réception des messages continus.

## 2. Test de portabilité (Caméra IA vers carte STM32 standard)

Ce script est conçu pour tester la réception des ordres de la caméra OpenMV par un microcontrôleur STM32 classique. Il prouve que le projet n'est pas strictement dépendant de la carte STeaMi et peut être adapté à d'autres robots basés sur l'écosystème STM32.

* **Fichier :** 
  * `stm32_uart_rx.ino` (Code Arduino pour STM32 générique)
* **Câblage :** Relier la broche de transmission (TX / P4) de la caméra à la broche de réception (RX) de la carte STM32. Relier impérativement les masses (GND) entre elles.
* **Usage :** Flasher la carte STM32 avec le code, placer un panneau devant la caméra OpenMV, et vérifier sur le moniteur série de la STM32 que les prédictions (V, I, S) sont correctement interprétées.