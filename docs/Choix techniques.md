# Rapport de R&D

**Projet :** Déploiement d'un modèle d'IA de vision sur microcontrôleur STM32N6 pour la robotique éducative.
**Objectif initial :** Intégrer un modèle de détection de panneaux routiers (YOLO/CNN entraîné sur le dataset GTSRB) sur l'architecture matérielle STM32N6 via les outils Siana Systems et le Discovery Kit ST.

---

## 1. Contexte et Cahier des Charges

Dans le cadre de la création d'un atelier d'intelligence artificielle embarquée (Edge AI), le système devait être capable de capturer un flux vidéo en temps réel, de détecter des panneaux de signalisation via un réseau de neurones, et de transmettre des ordres de pilotage à un robot (carte STeaMi). La solution devait être robuste, performante (>20 FPS) et, à terme, explicable dans un contexte pédagogique.

---

## 2. Phase 1 : Expérimentations sur la N6Cam (Siana Systems)

La première approche s'est concentrée sur l'utilisation de la caméra N6Cam fournie par Siana Systems et l'écosystème logiciel STM32CubeIDE couplé à X-CUBE-AI / ST Edge AI.

### 2.1. Les défis d'intégration et de compilation

L'intégration du modèle s'est avérée particulièrement fastidieuse en raison de l'architecture logicielle scindée (FSBL pour le bootloader et Application pour l'inférence). 
* **Manque de flexibilité des templates :** L'utilisation de modèles vierges générés par CubeIDE aboutissait à des crashs de la carte. Il a fallu cloner un template spécifique de Siana Systems, dont le code auto-généré s'attendait strictement à une architecture YOLOv8 pré-définie.
* **Processus de flashage anti-pédagogique :** L'obligation de signer manuellement les binaires (FSBL.bin et Application.bin) via des outils en ligne de commande masqués (STM32_SigningTool_CLI) rendait le processus de développement extrêmement lent et totalement inadapté à un public d'étudiants. Le seul modèle qu'il a été possible de faire fonctionner sur cette carte était un CNN qui était donc très mauvais pour la détection (car adapté à la classification) avec une performance d'une trentaine de FPS. Il a également été possible de faire fonctionner un modèle YOLO mais à 1/2 IPS, ce qui n'était pas adapté au projet. 

### 2.2. L'incompatibilité des fonctions d'activation (SiLU vs ReLU)

Lors des premiers tests d'inférence, un goulot d'étranglement majeur a été identifié : l'accélérateur matériel (NPU) de la carte ne gérait pas nativement la fonction d'activation SiLU utilisée par défaut dans YOLOv8.
* **Conséquence :** Le système déléguait ces calculs lourds au CPU (Software Fallback), provoquant un effondrement des performances (1 image toutes les 3 secondes) ou des crashs complets de la caméra.
* **Tentative de résolution :** Forçage de la fonction ReLU lors de l'entraînement sur Google Colab et tentatives de quantification en INT8. Bien que le modèle ait été allégé, le code "wrapper" de Siana Systems (le Post-Processing C++) n'était pas conçu pour s'adapter dynamiquement à des modèles modifiés ou à des architectures alternatives comme YOLOv5 ou YOLOX.

**Bilan Phase 1 :** L'écosystème Siana Systems souffre d'un manque de maturité logicielle (drivers instables) et impose un cadre de développement "boîte noire" trop rigide pour l'intégration de modèles personnalisés.

---

## 3. Phase 2 : Expérimentations sur le Discovery Kit (STM32N6570-DK)

Afin de contourner les limitations de la surcouche Siana Systems, le développement a été basculé sur la carte d'évaluation officielle de STMicroelectronics (Discovery Kit), avec l'objectif de développer l'application "Bare-Metal".

### 3.1. L'enfer des dépendances et de l'éditeur de liens (Linker)

La configuration du projet via le template Template_LRUN_AppS a nécessité l'importation manuelle de dizaines de librairies croisées (AI_Runtime, ATON, Evision, CACHEAXI). 
* Le système de gestion de projet d'Eclipse (CubeIDE) perdait régulièrement les liens symboliques et les variables d'environnement (STM32N657xx, LL_ATON_OSAL_BARE_METAL), obligeant à reconstruire l'arborescence matérielle (HAL) à la main pour réussir à compiler sans erreurs.

### 3.2. Le piège de l'exécution en RAM (HardFault / VTOR)

L'obstacle le plus complexe de cette phase a été la gestion de la mémoire lors du débogage. Le projet LRUN_AppS injectait le code directement dans la RAM (adresse 0x34000000) via la sonde ST-LINK, au lieu de la Flash.
* **Le Bug :** Lors de l'appel à la fonction de délai de la caméra (BSP_GetTick), le processeur cherchait la Table des Vecteurs d'interruption (VTOR). Or, le Linker avait placé les "poids" mathématiques du réseau de neurones à l'adresse de démarrage. Le processeur tentait d'exécuter l'IA comme du code source, sautait dans le vide (pointeur nul) et déclenchait un HardFault.
* **La Résolution partielle :** Il a fallu forcer la relocalisation dynamique de la table des vecteurs avec des barrières de synchronisation matérielles (SCB->VTOR = 0x34000400; __DSB(); __ISB();).

### 3.3. Le pare-feu matériel (RIF - Resource Isolation Framework)

Même avec un code IA compilant parfaitement, l'architecture TrustZone (sécurité matérielle) de la N6 bloquait l'accès aux périphériques. L'initialisation du capteur vidéo (IMX335) entraînait un crash car le bus I2C et l'interface DCMI étaient verrouillés par le système.
* Il a fallu écrire des dérogations matérielles manuelles (HAL_RIF_RISC_SetSlaveSecureAttributes) pour chaque GPIO et bus de communication.

**Bilan Phase 2 :** Bien que le Discovery Kit soit une machine surpuissante (inférence mesurée à 8.2ms), la complexité de la programmation bas niveau (gestion des interruptions en RAM, déverrouillage sécuritaire RIF, post-processing C++ écrit à la main) rend cette solution incompatible avec un prototypage rapide et un atelier pédagogique.

---

## 4. Conclusion Générale et Pivot Stratégique

Les essais sur les environnements pur ST/Siana ont démontré que la technologie STM32N6 est encore très orientée pour des ingénieurs experts en systèmes embarqués, nécessitant des mois de développement pour un simple Proof of Concept.

Pour répondre au cahier des charges, un pivot total vers l'écosystème OpenMV a été acté et validé avec succès.

### Pourquoi OpenMV est la solution de production :

1. **Élimination de la Toolchain C/C++ :** Le développement en MicroPython remplace les jours de configuration de CubeIDE et de gestion de la mémoire par de simples scripts lisibles et modifiables en direct.
2. **Gestion optimisée de la RAM (XIP) :** Le problème de RAM Overflow a été contourné en flashant le modèle directement dans la partition ROMFS de la caméra, permettant au NPU de lire l'IA sans surcharger la RAM.
3. **Quantification Hybride sur-mesure :** L'utilisation d'un modèle .tflite avec entrées en INT8 (pour la vitesse du NPU) et sorties en FLOAT32 (pour éviter le crash de l'interface graphique) a permis d'atteindre plus de 40 FPS.
4. **Communication Matérielle ("Hack" Jacdac) :** Au lieu de développer un bus I2C complexe en C++, le signal UART de la caméra a été physiquement routé dans le port Jacdac du robot STeaMi. En forçant la carte STeaMi en setHalfDuplex(), nous avons établi une communication unidirectionnelle d'une fiabilité totale.

**Verdict final :** Le projet est désormais 100% fonctionnel, reproductible, et pédagogiquement documentable grâce à l'abandon de l'architecture "Bare-Metal" au profit du tandem MicroPython (OpenMV) et C++ simplifié (Arduino IDE sur STeaMi).