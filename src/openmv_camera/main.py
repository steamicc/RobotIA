import csi
import time
import ml
from machine import UART 
from ml.postprocessing.ultralytics import YoloV8

# Initialisation de l'UART (Port 3 en général sur OpenMV, Baudrate: 115200)
# Vérifie la doc de ta N6CAM si l'UART n'est pas le numéro 3.
uart = UART(3, 115200)

NOM_DU_MODELE = "/rom/network_hybride.tflite"

print("Initialisation de la caméra...")
cam = csi.CSI()
cam.reset()

cam.pixformat(csi.RGB565)
cam.framesize(csi.QVGA)
cam.window((192, 192))

print("Chargement du modèle...")
net = ml.Model(NOM_DU_MODELE, postprocess=YoloV8(threshold=0.83))

net.labels = ["30km-h", "Interdit", "STOP"]

clock = time.clock()
print("🚀 Démarrage du flux vidéo multiclasse !")

def extraire_multiclasse(predictions):
    resultats = []
    if not isinstance(predictions, list):
        return resultats

    if len(predictions) == 1 and isinstance(predictions[0], list) and not (len(predictions[0]) > 0 and isinstance(predictions[0][0], tuple)):
        dossiers = predictions[0]
    else:
        dossiers = predictions

    for class_id, dossier in enumerate(dossiers):
        if isinstance(dossier, list):
            for obj in dossier:
                if isinstance(obj, tuple) and len(obj) >= 2:
                    rect = obj[0]
                    score = obj[1]
                    resultats.append((rect, score, class_id))
    return resultats

while True:
    clock.tick()
    img = cam.snapshot()

    predictions = net.predict([img])
    detections = extraire_multiclasse(predictions)

    # Variables pour l'envoi UART
    meilleur_score = 0
    ordre_a_envoyer = "N" # 'N' pour Rien par défaut

    for rect, score, class_id in detections:
        try:
            x, y, w, h = int(rect[0]), int(rect[1]), int(rect[2]), int(rect[3])
            img.draw_rectangle((x, y, w, h), color=(255, 0, 0), thickness=2)

            nom_classe = net.labels[class_id] if class_id < len(net.labels) else f"Classe {class_id}"
            pourcentage = int(score * 100)
            label = f"{nom_classe} {pourcentage}%"

            # Trace dans le terminal
            print(f"🎯 {label} détecté ! (Classe N°{class_id})")

            # Texte sur la vidéo
            text_y = max(0, y - 15)
            img.draw_string((x + 1, text_y + 1), label, color=(0, 0, 0), scale=2)
            img.draw_string((x, text_y), label, color=(255, 255, 255), scale=2)

            # --- Sélection du panneau avec la meilleure confiance ---
            if score > meilleur_score:
                meilleur_score = score
                if class_id == 0:   # 30km-h
                    ordre_a_envoyer = "V"
                elif class_id == 1: # Interdit
                    ordre_a_envoyer = "I"
                elif class_id == 2: # STOP
                    ordre_a_envoyer = "S"

        except Exception as e:
            print("Erreur de dessin :", e)

    # Envoi de l'ordre via UART ---
    # Le "\n" est vital ! Il indique à la STeaMi que le message est terminé.
    uart.write(ordre_a_envoyer + "\n")

    print("FPS: ", clock.fps())
