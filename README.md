# OLED Retro Pixel Racer

Retro Racer mit OLED Display 128x64  
[Makerblog.at](https://www.makerblog.at)

## Hardware-Anforderungen
- Arduino UNO R3 (oder vergleichbar)
- OLED I2C Display 128x64 mit SSD1306

## Verkabelung

OLED -> Arduino UNO R3
- SDA -> A4
- SCL -> A5
- GND -> GND
- VIN -> 5V

Push Button LINKS  
- GND -> Push Button -> D2

Push Button RECHTS  
- GND -> Push Button -> D3

Buzzer  
- GND -> Buzzer -> D6

## Notwendige Bibliotheken
Stelle sicher, dass die folgenden Bibliotheken über den Bibliotheks-Manager installiert sind:
- Adafruit SSD1306 einschließlich Adafruit GFX

## Optimierung
Um Gleitkommazahlen (float) zu vermeiden, werden alle relevanten Positionen und Geschwindigkeiten mit dem Faktor 10 skaliert und als int gespeichert. Dies verbessert die Performance und vermeidet Rundungsfehler.

### Skalierte Werte:
- Alle x- und y-Koordinaten werden mit *10 gespeichert.
- Alle Geschwindigkeiten sind mit *10 gespeichert.
- Zur Darstellung auf dem Display werden diese Werte durch 10 geteilt.

### Beispiel:
- Ein Auto mit x = 150 befindet sich in Wirklichkeit bei 15.0 Pixeln auf dem Display.
- Die Geschwindigkeit playerSpeed = 25 bedeutet eine Änderung von 2.5 px pro Frame.

Diese Skalierung wird in allen Berechnungen berücksichtigt, aber in der Darstellung (drawBitmap, drawFastHLine usw.) auf den echten Pixelwert zurückgerechnet (x / 10).
