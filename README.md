# Projet RobotIA : Détection de Panneaux (N6Cam & Robot STeaMi)

Bienvenue sur le dépôt officiel du projet de conduite autonome par vision pour le robot 4x4 STeaMi. 

Ce projet permet à un robot d'analyser son environnement en temps réel grâce à une intelligence artificielle embarquée (YOLOv8) et de réagir de manière autonome aux panneaux de signalisation routière.

## Résumé du Fonctionnement
1. **Vision :** Une caméra N6Cam (écosystème OpenMV) embarque un modèle IA entraîné sur mesure.
2. **Décision :** La caméra traite le flux vidéo à haute vitesse (>40 FPS), identifie le panneau le plus pertinent et détermine l'action à mener.
3. **Communication :** L'ordre est envoyé au robot via une liaison série (UART) transitant par un câble Jacdac détourné.
4. **Action :** Le microcontrôleur du robot STeaMi intercepte le signal, priorise ses capteurs de sécurité (bumpers), actionne ses moteurs et donne un retour visuel via son bandeau LED Neopixel.

## Matériel Requis
Pour reproduire ce projet, vous aurez besoin de :
* 1x Caméra N6Cam (OpenMV)
* 1x Robot 4x4 STeaMi (équipé d'une carte compatible Arduino)
* 1x Câble Jacdac **( Attention : Le fil 5V doit être impérativement isolé pour protéger la caméra qui fonctionne en 3.3V)**.

##  Navigation dans ce dépôt (Où trouver quoi ?)

Ce dépôt est structuré pour une prise en main rapide et une séparation claire entre le code, l'IA et la documentation :

*  **[docs/](./docs/)** : Contient le guide complet de déploiement technique et les ressources pédagogiques annexes. **C'est le dossier à consulter en priorité pour reproduire ou animer le projet.**
*  **[src/](./src/)** :
  * `openmv_camera/` : Le script `main.py` en MicroPython à flasher sur la caméra.
  * `steami_arduino/` : Le code source C++ complet (`.ino`) pour la carte STeaMi.
*  **[models/](./models/)** :
  * Le modèle `.tflite` (Quantification hybride : Entrée INT8 / Sortie FLOAT32) prêt à être déployé.
  * Le Notebook Google Colab (`yolov8_training.ipynb`) pour ré-entraîner ou adapter l'IA avec de nouveaux panneaux.

##  Démarrage Rapide (Quick Start)
1. Téléchargez le fichier `network_hybride.tflite` situé dans le dossier `models/`.
2. Flashez ce fichier dans la mémoire **ROMFS** de la caméra via OpenMV IDE.
3. Déposez le fichier `main.py` (issu du dossier `src/openmv_camera/`) sur la caméra.
4. Téléversez le code Arduino (issu du dossier `src/steami_arduino/`) sur le robot STeaMi.
5. Connectez la broche `P4` et le `GND` de la caméra au port Jacdac du robot.
6. Allumez le robot et placez un panneau devant la caméra !
