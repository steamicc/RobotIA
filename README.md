# 🚦 Projet YouthAILab : Détection de Panneaux (N6Cam & Robot STeaMi)

Bienvenue sur le dépôt officiel du projet de conduite autonome par vision pour le robot 4x4 STeaMi. 

Ce projet a été conçu pour les ateliers pédagogiques du YouthAILab. Il permet à un robot d'analyser son environnement en temps réel grâce à une intelligence artificielle embarquée (YOLOv8) et de réagir de manière autonome aux panneaux de signalisation routière.

## 🎯 Résumé du Fonctionnement
1. **Vision :** Une caméra N6Cam (écosystème OpenMV) embarque un modèle IA entraîné sur mesure.
2. **Décision :** La caméra traite le flux vidéo à haute vitesse (>40 FPS), identifie le panneau le plus pertinent et détermine l'action à mener.
3. **Communication :** L'ordre est envoyé au robot via une liaison série (UART) transitant par un câble Jacdac détourné.
4. **Action :** Le microcontrôleur du robot STeaMi intercepte le signal, priorise ses capteurs de sécurité (bumpers), actionne ses moteurs et donne un retour visuel via son bandeau LED Neopixel.

## 🛠️ Matériel Requis
Pour reproduire ce projet, vous aurez besoin de :
* 1x Caméra N6Cam (OpenMV)
* 1x Robot 4x4 STeaMi (équipé d'une carte compatible Arduino)
* 1x Câble Jacdac **(⚠️ Attention : Le fil 5V doit être impérativement isolé pour protéger la caméra qui fonctionne en 3.3V)**.

## 📂 Navigation dans ce dépôt (Où trouver quoi ?)

Ce dépôt est structuré pour répondre aux besoins des développeurs comme des animateurs d'ateliers :

* 📁 **[1_Guide_Deploiement](./1_Guide_Deploiement/)** : Contient le guide complet pas-à-pas (PDF) expliquant toute la démarche technique, de l'entraînement de l'IA jusqu'au code final. **C'est le document à lire en priorité si vous souhaitez reproduire le projet.**
* 📁 **[2_Codes_Sources](./2_Codes_Sources/)** :
  * `Camera_MicroPython` : Le script `main.py` à flasher sur la caméra.
  * `Robot_Arduino` : Le code source C++ complet pour la carte STeaMi.
  * `Modele_IA_Colab` : Le Notebook Google Colab d'entraînement YOLOv8 et le modèle `.tflite` pré-compilé.
* 📁 **[3_Ressources_Pedagogiques](./3_Ressources_Pedagogiques/)** : Contient les outils pour les animateurs (Fiche mémo de démarrage, schéma de câblage, explication des codes couleurs LED).

## 🚀 Démarrage Rapide (Quick Start)
1. Téléchargez le fichier `network_hybride.tflite` situé dans le dossier `2_Codes_Sources`.
2. Flashez ce fichier dans la mémoire **ROMFS** de la caméra via OpenMV IDE.
3. Déposez le fichier `main.py` sur la caméra.
4. Téléversez le code Arduino sur le robot STeaMi.
5. Connectez la broche `P4` et le `GND` de la caméra au port Jacdac du robot.
6. Allumez le robot et placez un panneau devant la caméra !

---
*Projet réalisé dans le cadre du développement des activités pédagogiques du YouthAILab - 2026.*