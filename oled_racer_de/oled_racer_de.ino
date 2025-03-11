/***************************************************************************
 Retro Racer mit OLED Display 128x64 - Kommentare in deutsch
 https://www.makerblog.at

 - Arduino UNO R3 (oder vergleichbar)
 - OLED I2C Display 128x64 mit SSD1306

 Verkabelung:
 
 OLED -> Arduino UNO R3
 SDA -> A4
 SVL -> A5
 GND -> GND
 VIN -> 5V

 Push Button LINKS   GND - Push Button - D2
 Push Button RECHTS  GND - Push Button - D3

 Buzzer              GND - Buzzer - D6

 Folgende Libraries müssen installiert sein, bei Fehlermeldung bitte mit dem Library Manager 
 kontrollieren

 - Adafruit SSD1306 inkl. Adafruit GFX

  Optimierung:
 -------------
 Um Gleitkommazahlen (float) zu vermeiden, werden alle relevanten Positionen 
 und Geschwindigkeiten mit dem Faktor 10 skaliert und als int gespeichert. 
 Dies verbessert die Performance und vermeidet Rundungsfehler.

 Skalierte Werte:
 - Alle x- und y-Koordinaten werden *10 gespeichert.
 - Alle Geschwindigkeiten sind *10 gespeichert.
 - Zur Darstellung auf dem Display werden diese Werte durch 10 geteilt .

 Beispiel:
 - Ein Auto mit x = 150 befindet sich in Wirklichkeit bei 15.0 Pixeln auf dem Display.
 - Die Geschwindigkeit playerSpeed = 25 bedeutet eine Änderung von 2.5 px pro Frame.

 Diese Skalierung wird in allen Berechnungen berücksichtigt, aber 
 in der Darstellung (drawBitmap, drawFastHLine usw.) auf den echten Pixelwert 
 zurückgerechnet (x / 10).
 
 ***************************************************************************/

#include <avr/pgmspace.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Einstellungen für das OLED Display mit SSD1306 Controller
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_RESET = -1;
const int SCREEN_ADDRESS = 0x3C;  // I2C Adresse des Displays, siehe Datenblatt
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const bool DEBUG_PRINT = false;  // auf true für Debug-Ausgaben am seriellen Monitor

// Spieler und Fahrspuren
const int PLAYER_X = 10;      // Position X des Spielerautos
const int PLAYER_WIDTH = 10;  // Breite der Spielefigur in px
const int PLAYER_HEIGHT = 8;  // Höhe der Autos in px

const int LANES = 4;                              // Anzahl der Spuren
const int LANE_HEIGHT = (SCREEN_HEIGHT / LANES);  // Höhe einer Spur
const int OBSTACLE_WIDTH = 10;                    // Breite der Gegner in px
const int OBSTACLE_HEIGHT = 8;                    // Höhe der Gegner in px
const int MAX_OBSTACLES = 10;                     // maximale Anzahl der möglichen Gegnerautos

const int BUTTON_LEFT = 2;   // Push button LINKS an Digitalpin 2
const int BUTTON_RIGHT = 3;  // Push button RECHTS an Digitalpin 3
const int PIN_BUZZER = 6;    // Buzzer an Digitalpin 6

const int FRAME_TIME = 40;  // Ziel-Framerate: 40 ms pro Frame = 25 FPS
long frameCounter = 0;
unsigned long lastFrameTime = 0;

const int GAME_OVER_DELAY = 1000;  // 1 Sekunde Sperrzeit nach Game Over
unsigned long gameOverTime = 0;    // Speichert den Zeitpunkt des Game Over

// Enumeration für mögliche Spielzustände
enum GameState { STARTSCREEN,
                 HIGHSCORE,
                 PLAYING };
GameState gameState = STARTSCREEN;

// Achtung: x-Koordinaten, Geschwindigkeiten und zurückgelegte Strecken sind um Faktor 10 hochskaliert.
// Berechnungen erfolgen mit int, um Gleitkommazahlen zu vermeiden.
// Vor der Darstellung auf dem Display wird durch 10 geteilt.

// Datentyp für Eigenschaften eines Gegners
struct Obstacle {
  int x;
  int lane;
  int cartype;
  bool active;
};

// High Score Liste
long highscores[10] = {};
int newHighscoreIndex = -1;
unsigned long highscoreTime = 0;
unsigned long lastBlinkTime = 0;
bool showBlink = true;

// Intro Screen Animation
const int BITMAP_SPEED = 5;  // Pixels per frame
int introBitmapX1 = 12;      // Bild 1 Start off-screen (left)
int introBitmapX2 = -52;     // Bild 2 Start off-screen (left)
unsigned long lastBitmapMove = 0;

// Eigenschaften für Spieler, Gegner und Fahrspuren
Obstacle obstacles[MAX_OBSTACLES];  // Array für Gegner
int playerLane = 2;                 // Spieler startet in Spur 2
bool gameOver = false;
int spawnProbability = 5;                  // Start-Wahrscheinlichkeit für neue Gegner (1-100)
int maxActiveObstacles = 5;                // Start mit 5 Gegnern
int dashedLineOffset = 0;                  // Offset für die Bewegung der gestrichelten Linien
long distanceCounter = 0;                  // Gesamt zurückgelegte Strecke in Pixeln
int playerSpeed = 20;                      // Geschwindigkeit der Hindernisse
int laneSpeeds[LANES] = { 13, 10, 8, 5 };  // Geschwindigkeiten in px mit Faktor 10
int laneCooldown[LANES] = { 0 };           // Zähler für Sperrzeit pro Spur
const int MIN_FRAMES_BETWEEN_SPAWNS = 20;  // Mindestwartezeit pro Spur vor neuem Gegner

const int DEBOUNCE_FRAMES = 3;  // Anzahl der Frames, in denen kein erneuter Button-Input akzeptiert wird
int debounceCounter = 0;

int lastBuzzerFreq = 0;  // Letzte verwendete Frequenz für Buzzer


// Grafiken für Intro, Spieler und Gegner
// konvertiert aus PNG mit https://javl.github.io/image2cpp/

const unsigned char intro_bitmap_[] PROGMEM = {
  // 'player_car, 40x24px
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfe, 0x00, 0x00, 0x00,
  0x30, 0x03, 0x00, 0x00, 0x01, 0xe0, 0x01, 0x80, 0x00, 0x02, 0x20, 0x01, 0xff, 0x00, 0x04, 0x20,
  0x01, 0xc0, 0x80, 0x08, 0x20, 0x01, 0xc0, 0xf0, 0x33, 0x30, 0x03, 0xc0, 0x08, 0x23, 0x1f, 0xfe,
  0xc1, 0xcc, 0x23, 0x0c, 0xfc, 0xc1, 0xe4, 0x23, 0x1c, 0xfe, 0x40, 0x04, 0x23, 0x3c, 0xff, 0x03,
  0x84, 0x22, 0x3c, 0xff, 0x03, 0xc4, 0x20, 0x00, 0x00, 0x00, 0x04, 0x20, 0x00, 0x00, 0x00, 0x0c,
  0x21, 0xe0, 0x00, 0x1e, 0x08, 0x33, 0x30, 0x00, 0x33, 0x10, 0x3e, 0x10, 0x00, 0x21, 0x30, 0x1e,
  0x1f, 0xff, 0xe1, 0xe0, 0x03, 0x30, 0x00, 0x33, 0x00, 0x01, 0xe0, 0x00, 0x1e, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char player_car_[] PROGMEM = {
  // 'player_car, 12x19px
  0x33, 0x00, 0x7f, 0x80, 0xc4, 0xc0, 0x56, 0x40, 0x56, 0x40, 0xc4, 0xc0, 0x7f, 0x80, 0x33, 0x00
};

// 'car_8x10_1', 10x8px
const unsigned char car_car_8x10_1[] PROGMEM = {
  0x63, 0x00, 0xff, 0xc0, 0x82, 0x40, 0x83, 0x40, 0x83, 0x40, 0x82, 0x40, 0xff, 0xc0, 0x63, 0x00
};
// 'car_8x10_2', 10x8px
const unsigned char car_car_8x10_2[] PROGMEM = {
  0x63, 0x00, 0xf7, 0x80, 0x9e, 0x40, 0x83, 0x40, 0x83, 0x40, 0x9e, 0x40, 0xf7, 0x80, 0x63, 0x00
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 64)
const int car_allArray_LEN = 2;
const unsigned char* car_allArray[2] = {
  car_car_8x10_1,
  car_car_8x10_2
};

// Setup und Game Loop

void setup() {
  Serial.begin(9600);

  // Display initialisieren oder Pause bei Fehler
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1) {
      delay(100);
    }
  }

  // Interne PULLUP-Widerstände für Pushbuttons aktivieren
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);

  pinMode(PIN_BUZZER, OUTPUT);

  randomSeed(analogRead(0));

  // Spieldaten zurücksetzen und Status auf STARTSCREEN
  resetGame();
  gameState = STARTSCREEN;
}

void loop() {
  unsigned long currentTime = millis();

  // Wenn FRAME_TIME vergangen, dannn nächster Frame
  if (currentTime - lastFrameTime >= FRAME_TIME) {

    // Ausgabe der Framezeit, falls nötig:
    if (DEBUG_PRINT) {
      Serial.print("FR: ");
      Serial.println(currentTime - lastFrameTime);
    }

    lastFrameTime = currentTime;

    switch (gameState) {
      case STARTSCREEN:
        renderStartScreen();
        if (anyKeyPressed()) {
          resetGame();
          gameState = PLAYING;
        }
        break;

      case PLAYING:
        // Wenn gameOver, dann je nach verstrichener Zeit Zustand wechseln
        if (gameOver) {
          if (currentTime - gameOverTime > 3000) {  // 3 Sekunden nach Game Over zum Highscore-Screen
            gameState = HIGHSCORE;
            highscoreTime = millis();
          }
          // 1 Sekunde lang Buttonklicks ignorieren, dann neue Runde startbar
          if ((currentTime - gameOverTime > 1000) && anyKeyPressed()) {
            resetGame();
            gameState = PLAYING;
          }
        } else {
          handleInput();
          updateGameLogic();
        }
        renderGame();
        break;

      case HIGHSCORE:
        renderHighscoreScreen();
        if (currentTime - gameOverTime > 8000) {  // Nach 3s Game Over + 5s im Highscore-Screen zum Startscreen
          gameState = STARTSCREEN;
        }
        if (anyKeyPressed()) {
          resetGame();
          gameState = PLAYING;
        }
        break;
    }
  }
}

// ---- Spielsteuerung und Logik ------ //

// PULLUP-Logik: LOW bedeutet gedrückt
bool anyKeyPressed() {
  return digitalRead(BUTTON_LEFT) == LOW || digitalRead(BUTTON_RIGHT) == LOW;
}

void handleInput() {
  if (debounceCounter > 0) {
    debounceCounter--;
    return;  // Während Debounce-Phase keine Eingaben akzeptieren
  }

  // Spurwechsel nach links nur wenn nicht schon ganz links
  if (digitalRead(BUTTON_LEFT) == LOW && playerLane > 0) {
    playerLane--;
    debounceCounter = DEBOUNCE_FRAMES;
  }

  // Spurwechsel nach rechts nur wenn nicht schon ganz rechts
  if (digitalRead(BUTTON_RIGHT) == LOW && playerLane < LANES - 1) {
    playerLane++;
    debounceCounter = DEBOUNCE_FRAMES;
  }
}

// ---- Spiellogik im aktuellen Frame ----- //
void updateGameLogic() {
  updateObstacles();
  updateBuzzer();
  spawnNewObstacle();
  checkCollision();
  updateDashedLines();

  // Geschwindigkeitserhöhung alle 20 Frames
  frameCounter++;
  if (frameCounter % 20 == 0) {  // Alle 20 Frames
    playerSpeed++;
  }

  // Cooldown pro Spur, damit nicht 2 Gegner unmittelbar hintereinander auf der gleichenm Spur spawnen
  for (int i = 0; i < LANES; i++) {
    if (laneCooldown[i] > 0) {
      laneCooldown[i]--;
    }
  }

  // Zu Spielbeginn nur 1 Gegner, bis 4 Gegner gleichzeitig rasch anwachsend, dann alle 500 Pixel Bewegung (ca. 4 Displaybreiten) ein Gegner mehr.
  if (distanceCounter < 500) {
    maxActiveObstacles = 1;
  } else if (distanceCounter < 1000) {
    maxActiveObstacles = 2;
  } else if (distanceCounter < 2000) {
    maxActiveObstacles = 3;
  } else if (distanceCounter < 3000) {
    maxActiveObstacles = 4;
  } else  // Erhöhe max. Gegneranzahl alle 5000 Distanz (= 500 Pixel) bis maximal 10
    if (distanceCounter / 5000 + 5 <= 10) {
      maxActiveObstacles = distanceCounter / 5000 + 5;
    }
}

// ---- Kollisions-Check zwischen Spieler und Gegnern ----- //
void checkCollision() {
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    // collisionMargin verringert den Bereich, der eine Kollision verursacht, ermöglicht "knappe" Überholmanöver
    int collisionMargin = 2 * 10;  // Skalierungsfaktor *10 berücksichtigen
    if ((obstacles[i].active && obstacles[i].x <= (PLAYER_X * 10 + PLAYER_WIDTH * 10 - collisionMargin) && (obstacles[i].x + OBSTACLE_WIDTH * 10) >= (PLAYER_X * 10 + collisionMargin) && obstacles[i].lane == playerLane)) {
      gameOver = true; // Kollision erkannt
      gameOverTime = millis();
      saveHighscores(distanceCounter); // Aktuellen Score in Highscore-Liste eintragen
      noTone(PIN_BUZZER);  // Tonausgabe stopppen
    }
  }

  // Wenn keine Kollision in diesem Frame erkannt, Score (= Distanz) erhöhen
  if (!gameOver) {
    distanceCounter += playerSpeed;
  }
}

// ---- Spieleigenschaften zurücksetzen für neue Runde ----- //
void resetGame() {
  gameOver = false;
  playerLane = 2;        // Spieler startet in der 3. Spur von links
  playerSpeed = 15;      // Geschwindigkeit zurücksetzen
  spawnProbability = 5;  // Spawnrate zurücksetzen
  frameCounter = 0;
  dashedLineOffset = 0;
  distanceCounter = 0;
  maxActiveObstacles = 5;  // Start mit 5 Gegnern
  for (int i = 0; i < LANES; i++) {
    laneCooldown[i] = 0;  // Alle Spuren sofort freigeben
  }
  resetObstacles();
}


// ---- Gegner bewegen und ggf resetten ----- //
// Gegner bewegen sich in die gleiche Richtung wie der Spieler, aber mit geringerer Geschwindigkeit
// Dadurch kommt der Spieler den Gegnern mit der Differenz der Geschwindigkeiten näher
// Fahrspuren haben unterschiedliche Geschwindigkeiten, gespeichert in laneSpeeds[]
void updateObstacles() {
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      // Neue Position des Gegners relativ zum Spieler anhand Geschwindigkeitsunterschied berechnen
      obstacles[i].x = obstacles[i].x - playerSpeed + laneSpeeds[obstacles[i].lane];
      // Wenn Gegner den Anzeigebereich verlässt, dann deaktivieren (Skalierungsfaktor 10!)
      if (obstacles[i].x < -OBSTACLE_WIDTH * 10) {
        obstacles[i].active = false;
      }
    }
  }
}

// ---- Zufällige Erzeugung neuer Gegner ----- //
void spawnNewObstacle() {
  // Zähle aktuell aktive Gegner
  int activeObstacles = 0;
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      activeObstacles++;
    }
  }

  // Prüfe, ob die aktuelle maximale Anzahl an Gegnern bereits erreicht ist
  if (activeObstacles >= maxActiveObstacles) {
    return;  // Keine neuen Gegner spawnen, wenn Limit erreicht
  }

  // Wähle zufällige Spur für neuen Gegner
  int selectedLane = random(0, LANES);

  // Prüfe, ob die Spur eine Cooldown-Phase hat
  // Cooldown verhindert, dass zwei Gegner unmittelbar hintereinander auf selber Spur spawnen
  if (laneCooldown[selectedLane] > 0) {
    return;  // Kein Spawn auf dieser Spur, wenn noch Cooldown aktiv ist
  }

  // Suche nach freiem Slot im Array für einen neuen Gegner
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (!obstacles[i].active) {                     // Falls Slot frei
      if (random(100) < spawnProbability) {         // Zufällige Wahrscheinlichkeit für neuen Gegner
        obstacles[i].x = (SCREEN_WIDTH + 10) * 10;  // Startposition außerhalb des Bildschirms
        obstacles[i].lane = selectedLane;           // Gewählte Spur
        obstacles[i].cartype = random(2);           // Zufälliges Sprite
        obstacles[i].active = true;

        laneCooldown[selectedLane] = MIN_FRAMES_BETWEEN_SPAWNS;  // Cooldown für diese Spur setzen
        return;
      }
    }
  }
}

// ---- Alle Hindernisse initialisieren ------ //
void resetObstacles() {
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    obstacles[i].x = 0; // X-Position = Position entlang der Fahrspur
    obstacles[i].lane = 0; // zugewiesene Fahrspur
    obstacles[i].cartype = 0; // 2 verschiedene Sprites
    obstacles[i].active = false; // gerade aktiv oder nicht?
  }
}

// ---- Rendering des Spielfelds im aktuellen Frame ------ //
void renderGame() {
  display.clearDisplay(); // Display löschen
  drawDashedLines(); // Trennlinien zeichnen
  drawObstacles(); // Gegner zeichnen
  drawPlayer(); // Spieler zeichnen
  if (gameOver) drawGameOver(); // Wenn Game over, dann Overlay ausgeben
  drawDistanceCounter(); // Punktezahl anzeigen
  display.display(); // Displayinhalt ausgeben
}

// ---- Gestrichelte Linie mit Spielergeschwindigkeit rückwärts bewegen ------ //
void updateDashedLines() {
  dashedLineOffset -= playerSpeed;
  if (dashedLineOffset < -100) {
    dashedLineOffset = 0;
  }
}

// ---- Startscreen mit Animation ausgeben ------ //
void renderStartScreen() {
  display.clearDisplay();
  display.setRotation(1);

  // Position zweier Bitmaps berechnen, soll wie ein endlos durchfahrendes Auto aussehen 
  if (millis() - lastBitmapMove > 50) {        // Alle 50ms ein Update
    introBitmapX1 += BITMAP_SPEED;             // Bitmap 1 nach rechts bewegen
    if (introBitmapX1 > SCREEN_HEIGHT + 12) {  // Position nach links außerhalb zurücksetzen, wenn rechts aus dem Bild
      introBitmapX1 = -52;
    }
    introBitmapX2 += BITMAP_SPEED;             // Bitmap 2 (gleiches Bild, aber nach rechts versetzt) nach rechts bewegen
    if (introBitmapX2 > SCREEN_HEIGHT + 12) {  // Position nach links außerhalb zurücksetzen, wenn rechts aus dem Bild
      introBitmapX2 = -52;
    }
    lastBitmapMove = millis();
  }

  // Beide Bitmaps anzeigen, erzeugt Endlosbewegung von links nach rechts
  display.drawBitmap(introBitmapX1, 12, intro_bitmap_, 40, 24, SSD1306_WHITE);
  display.drawBitmap(introBitmapX2, 12, intro_bitmap_, 40, 24, SSD1306_WHITE);

  // Texte auf Startseite anzeigen
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 46);
  display.println("Pixel");
  display.setCursor(3, 66);
  display.println("Racer");
  display.setTextSize(1);

  display.setCursor(8, 96);
  display.println("Click to");
  display.setCursor(3, 106);
  display.println("start game");
  display.setRotation(0);
  display.display();
}

// ---- Top 10 Highscores ausgeben ------ //
void renderHighscoreScreen() {
  display.clearDisplay();
  display.setRotation(1); // Textausgabe um 90 Grad rotiert wegen Montage am Breadboard
  display.setCursor(2, 6);
  display.drawFastHLine(2, 16, 60, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("HIGHSCORES");

  // Aktuell erreichter Score soll blinken, wenn in Top10, Blinkstatus alle 100ms ändern
  if (millis() - lastBlinkTime > 100) {
    showBlink = !showBlink;
    lastBlinkTime = millis();
  }
  // Liste ausgeben, aktuellen Score blinken lassen falls in Liste enthalten
  for (int i = 0; i < 10; i++) {
    display.setCursor(10, 20 + (i * 10)); // Cursor an Position in der Liste
    display.print(i + 1);
    display.print(". ");

    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    char scoreBuffer[10];  // Buffer für die Zahl als String
    sprintf(scoreBuffer, "%ld", highscores[i] / 10); // Zahl in String umwandeln
    display.getTextBounds(scoreBuffer, 0, 0, &x1, &y1, &textWidth, &textHeight); // Breite des Scores berechnen
    display.setCursor(55 - textWidth, 20 + (i * 10)); // Cursor setzen für linksbündige Anzeige

    // Wenn aktueller Score, dann blinken (=alle 100ms nicht anzeigen)
    // Das continue beendet in dem Fall diesen Durchlauf der for-Schleife, springt sofort zum nächsten Durchlauf, 
    // dadurch wird der Score in der folgenden Zeile alle 100ms nicht ausgegeben und blinkt
    if (i == newHighscoreIndex && !showBlink) {
      continue;   
    }
    display.print(highscores[i] / 10);  // Score = zurückgelegte Distanz durch Skalierungsfaktor 10 geteilt
  }

  display.setRotation(0); // Rotation zurücksetzen
  display.display(); // Displayinmhalt ausgeben
}


// ---- Gestrichelte Linien zwischen Fahrspuren zeichnen ------ //
void drawDashedLines() {
  // Gestrichelte Linien zwischen den Spuren
  for (int i = 1; i < LANES; i++) {
    for (int j = 0; j < SCREEN_WIDTH; j += 10) {
      // dashedLineOffset wird pro Frame in updateDashedLines() verringert , dadurch "bewegt" sich die Linie
      display.drawFastHLine((j + dashedLineOffset / 10) % SCREEN_WIDTH, i * LANE_HEIGHT, 6, SSD1306_WHITE);
    }
  }
}

// ---- Gegnersprites ausgeben ------ //
void drawObstacles() {
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      // x-Koordinate wird durch Skalierungsfaktor 10 geteilt, um in echten Pixeln zu zeichnen
      display.drawBitmap(obstacles[i].x / 10, lanePosition(obstacles[i].lane), car_allArray[obstacles[i].cartype], 10, 8, SSD1306_WHITE);
    }
  }
}

// ---- Spielersprite an aktuelle Position ------ //
void drawPlayer() {
  display.drawBitmap(PLAYER_X, lanePosition(playerLane), player_car_, 10, 8, SSD1306_WHITE);
}

// ---- Aktuelle Distanz/Punktezahl oben ins Display schreiben ------ //
void drawDistanceCounter() {
  display.setRotation(1); // Display steckt 90 Grad gedreht am Breadboard, Rotation für folgende Ausgabe anpassen
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t textWidth, textHeight;
  char scoreBuffer[10];  // Buffer für die Zahl als String
  sprintf(scoreBuffer, "%ld", distanceCounter / 10); // Wandelt Zahl in String um, dividiert durch Skalierungsfaktor 10
  display.getTextBounds(scoreBuffer, 0, 0, &x1, &y1, &textWidth, &textHeight); // Score-Breite berechnen für Zentrierung
  display.fillRect(SCREEN_HEIGHT / 2 - textWidth / 2 - 2, 0, textWidth + 4, 8, SSD1306_BLACK); // Rechteck für Punktestand leeren
  display.setCursor(SCREEN_HEIGHT / 2 - textWidth / 2, 0); // Cursor setzen, Score mittig ausgeben
  display.print(distanceCounter / 10); // Um Skalierungsfaktor 10 verringern
  display.setRotation(0);
}

// ---- Game over Overlay über Spielfeld anzeigen ------ //
void drawGameOver() {
  display.setRotation(1); // Display steckt 90 Grad gedreht am Breadboard, Rotation für folgende Ausgabe anpassen
  display.fillRect(1, 30, 62, 60, SSD1306_WHITE);
  display.setCursor(4, 34);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.println("GAME OVER");
  display.setCursor(3, 50);
  display.println("Your score");
  display.setCursor(16, 66);

  int16_t x1, y1;
  uint16_t textWidth, textHeight;
  char scoreBuffer[10];  // Buffer für die Zahl als String
  sprintf(scoreBuffer, "%ld", distanceCounter / 10); // Wandelt Zahl in String um, dividiert durch Saklierungsfaktor 10
  display.setTextSize(2);
  display.getTextBounds(scoreBuffer, 0, 0, &x1, &y1, &textWidth, &textHeight); // Score-Breite berechnen für Zentrierung
  display.setCursor(32 - textWidth / 2, 66);
  display.println(distanceCounter / 10); // Durch Skalierungsfaktor 10 dividieren
  display.setRotation(0);
}


// ---- Highscore in Liste der Top10 einsortieren ------ //
void saveHighscores(long newScore) {
  newHighscoreIndex = -1;  // Position speichern, falls Ergebnis in Top 10
  for (int i = 0; i < 10; i++) {     // Neuen Score in das richtige Ranking einfügen
    if (newScore > highscores[i]) {  // Neuer Score ist besser?
      for (int j = 9; j > i; j--) {  // Alte Werte nach unten verschieben
        highscores[j] = highscores[j - 1];
      }
      highscores[i] = newScore;  // Neuen Highscore einfügen
      newHighscoreIndex = i;     // Index des neuen Scores speichern für Blink-Animation
      break;
    }
  }
}

// ---- Buzzerfrequenz langsam mit playerSpeed ändern ------ //
void updateBuzzer() {
  int buzzerFreq = map(playerSpeed, 10, 100, 50, 70);  // Map playerSpeed zu Frequenz
  if (buzzerFreq != lastBuzzerFreq) {                  // Nur updaten wenn sich Frequenz geändert hat
    tone(PIN_BUZZER, buzzerFreq);
    lastBuzzerFreq = buzzerFreq;
  }
}

// ---- Y-Positionen für Autos auf Fahrspuren berechnen ------ //
int lanePosition(int lane) {
  return lane * LANE_HEIGHT + (LANE_HEIGHT - PLAYER_HEIGHT) / 2;
}

