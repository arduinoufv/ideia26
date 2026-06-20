#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>

// ─── Configurações da Rede e MQTT ─────────────────────────
const char* ssid = "moto g(7) 4767";
const char* password = "arduino351";
const char* mqtt_server = "broker.mqtt-dashboard.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "snake/control";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ─── Configurações do Painel de LEDs ──────────────────────
#define LED_PIN 13
#define MATRIX_W 16
#define MATRIX_H 16
#define NUM_LEDS (MATRIX_W * MATRIX_H)
#define BRIGHTNESS 60
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

// ─── Touch capacitivo ─────────────────────────────────────
#define TOUCH_UP T5     // GPIO 12 - Sobe (Verde)
#define TOUCH_DOWN T6   // GPIO 14 - Desce (Vermelho)
#define TOUCH_ENTER T7  // GPIO 27 - Entrar/Sair (Azul)
#define TOUCH_THRESH 40

// ─── Máquina de Estados e Controle ────────────────────────
enum State { STATE_MENU, STATE_PLAYING };
State currentState = STATE_MENU;

int selectedGame = 0; // 0: Snake, 1: Tetris, 2: Breakout
volatile int mqttCmd = 0; // 0=Nenhum, 1=Up, 2=Down, 3=Left, 4=Right

// Timers para não travar a rede
unsigned long lastWiFiAttempt = 0;
unsigned long lastMqttAttempt = 0;

// ══════════════════════════════════════════════════════════
// FUNÇÕES AUXILIARES DA MATRIZ
// ══════════════════════════════════════════════════════════
void drawPixel(int x, int y, CRGB color) {
  if (x >= 0 && x < MATRIX_W && y >= 0 && y < MATRIX_H) {
    uint16_t index;
    if (y % 2 == 0) index = y * MATRIX_W + x;
    else index = y * MATRIX_W + (MATRIX_W - 1 - x);
    leds[index] = color;
  }
}

// ══════════════════════════════════════════════════════════
// CONEXÃO WI-FI E MQTT (Não bloqueante)
// ══════════════════════════════════════════════════════════
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  msg.toLowerCase();
  
  if (msg == "up") mqttCmd = 1;
  else if (msg == "down") mqttCmd = 2;
  else if (msg == "left") mqttCmd = 3;
  else if (msg == "right") mqttCmd = 4;
}

void handleNetwork() {
  unsigned long now = millis();

  // Tenta conectar Wi-Fi a cada 5 segundos
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWiFiAttempt > 5000) {
      lastWiFiAttempt = now;
      WiFi.begin(ssid, password);
    }
    return; // Se não tem Wi-Fi, não tenta MQTT
  }

  // Tenta conectar MQTT a cada 5 segundos
  if (!mqttClient.connected()) {
    if (now - lastMqttAttempt > 5000) {
      lastMqttAttempt = now;
      // Cria ID único para não dar conflito no broker público
      String clientId = "ESP32_Matrix_";
      clientId += String(random(0xffff), HEX);
      
      if (mqttClient.connect(clientId.c_str())) {
        mqttClient.subscribe(mqtt_topic);
      }
    }
  } else {
    // Se está conectado, mantém o MQTT vivo
    mqttClient.loop();
  }
}

// ══════════════════════════════════════════════════════════
// JOGO 1 — SNAKE
// ══════════════════════════════════════════════════════════
#define SNAKE_LEN 64
#define SNAKE_SPEED 150
struct Point { int8_t x, y; };

Point snakeBody[SNAKE_LEN];
int8_t snakeDir = 0; // 0: Dir, 1: Baixo, 2: Esq, 3: Cima
uint8_t snakeLen = 4;
Point snakeFood;
unsigned long snakeTimer = 0;

void snakeRandFood() {
  snakeFood.x = random(MATRIX_W);
  snakeFood.y = random(MATRIX_H);
}

void snakeInit() {
  snakeLen = 4; snakeDir = 0;
  for (uint8_t i = 0; i < snakeLen; i++) {
    snakeBody[i].x = snakeLen - 1 - i;
    snakeBody[i].y = MATRIX_H / 2;
  }
  snakeRandFood();
  mqttCmd = 0;
}

void snakeLogic() {
  if (mqttCmd == 1 && snakeDir != 1) snakeDir = 3; // Up
  if (mqttCmd == 2 && snakeDir != 3) snakeDir = 1; // Down
  if (mqttCmd == 3 && snakeDir != 0) snakeDir = 2; // Left
  if (mqttCmd == 4 && snakeDir != 2) snakeDir = 0; // Right
  mqttCmd = 0;

  if (millis() - snakeTimer >= SNAKE_SPEED) {
    snakeTimer = millis();
    Point head = snakeBody[0];
    
    if (snakeDir == 0) head.x++;
    else if (snakeDir == 1) head.y++;
    else if (snakeDir == 2) head.x--;
    else head.y--;
    
    if (head.x < 0) head.x = MATRIX_W - 1;
    if (head.x >= MATRIX_W) head.x = 0;
    if (head.y < 0) head.y = MATRIX_H - 1;
    if (head.y >= MATRIX_H) head.y = 0;

    for (uint8_t i = 0; i < snakeLen; i++) {
      if (head.x == snakeBody[i].x && head.y == snakeBody[i].y) { snakeInit(); return; }
    }

    bool ate = (head.x == snakeFood.x && head.y == snakeFood.y);
    for (int8_t i = min((int)snakeLen - 1, SNAKE_LEN - 2); i >= 0; i--) {
      snakeBody[i + 1] = snakeBody[i];
    }
    snakeBody[0] = head;
    
    if (ate) {
      if (snakeLen < SNAKE_LEN) snakeLen++;
      snakeRandFood();
    }
  }

  if ((millis() / 250) % 2 == 0) drawPixel(snakeFood.x, snakeFood.y, CRGB::Red);
  for (uint8_t i = 0; i < snakeLen; i++) drawPixel(snakeBody[i].x, snakeBody[i].y, CRGB::Green);
  drawPixel(snakeBody[0].x, snakeBody[0].y, CRGB::White);
}

// ══════════════════════════════════════════════════════════
// JOGO 2 — TETRIS (Simplificado)
// ══════════════════════════════════════════════════════════
#define TETRIS_SPEED 200
uint8_t tetrisBoard[MATRIX_H][MATRIX_W];
Point tetrisPiece[4];
unsigned long tetrisTimer = 0;

void tetrisNewPiece() {
  int sx = MATRIX_W / 2 - 1;
  tetrisPiece[0] = {sx, 0}; tetrisPiece[1] = {sx+1, 0};
  tetrisPiece[2] = {sx, 1}; tetrisPiece[3] = {sx+1, 1};
}

void tetrisInit() {
  memset(tetrisBoard, 0, sizeof(tetrisBoard));
  tetrisNewPiece();
  mqttCmd = 0;
}

bool tetrisCanMove(int8_t dx, int8_t dy) {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t nx = tetrisPiece[i].x + dx;
    int8_t ny = tetrisPiece[i].y + dy;
    if (nx < 0 || nx >= MATRIX_W || ny >= MATRIX_H) return false;
    if (ny >= 0 && tetrisBoard[ny][nx]) return false;
  }
  return true;
}

void tetrisLogic() {
  int8_t moveX = 0;
  if (mqttCmd == 3) moveX = -1;
  if (mqttCmd == 4) moveX = 1;
  if (mqttCmd == 2) tetrisTimer = 0;
  mqttCmd = 0;

  if (moveX != 0 && tetrisCanMove(moveX, 0)) {
    for (uint8_t i = 0; i < 4; i++) tetrisPiece[i].x += moveX;
  }

  if (millis() - tetrisTimer >= TETRIS_SPEED) {
    tetrisTimer = millis();
    if (tetrisCanMove(0, 1)) {
      for (uint8_t i = 0; i < 4; i++) tetrisPiece[i].y++;
    } else {
      for (uint8_t i = 0; i < 4; i++) {
        if (tetrisPiece[i].y >= 0) tetrisBoard[tetrisPiece[i].y][tetrisPiece[i].x] = 1;
      }
      tetrisNewPiece();
      
      for (int8_t r = MATRIX_H - 1; r >= 0; r--) {
        bool full = true;
        for (uint8_t c = 0; c < MATRIX_W; c++) if (!tetrisBoard[r][c]) full = false;
        if (full) {
          for (int8_t r2 = r; r2 > 0; r2--) memcpy(tetrisBoard[r2], tetrisBoard[r2-1], MATRIX_W);
          memset(tetrisBoard[0], 0, MATRIX_W);
          r++; 
        }
      }
    }
  }

  for (uint8_t y = 0; y < MATRIX_H; y++) {
    for (uint8_t x = 0; x < MATRIX_W; x++) {
      if (tetrisBoard[y][x]) drawPixel(x, y, CRGB::Blue);
    }
  }
  for (uint8_t i = 0; i < 4; i++) {
    if (tetrisPiece[i].y >= 0) drawPixel(tetrisPiece[i].x, tetrisPiece[i].y, CRGB::DeepSkyBlue);
  }
}

// ══════════════════════════════════════════════════════════
// JOGO 3 — BREAKOUT
// ══════════════════════════════════════════════════════════
int paddleX = 6;
float ballX = 8.0, ballY = 12.0;
float ballDX = 0.5, ballDY = -0.5;
bool bricks[4][16];

void breakoutInit() {
  paddleX = 6; ballX = 8.0; ballY = 12.0; 
  ballDX = 0.5; ballDY = -0.5;
  for (int y=0; y<4; y++) for (int x=0; x<16; x++) bricks[y][x] = true;
  mqttCmd = 0;
}

void breakoutLogic() {
  if (mqttCmd == 3) paddleX = max(0, paddleX - 1);
  if (mqttCmd == 4) paddleX = min(MATRIX_W - 3, paddleX + 1);
  mqttCmd = 0;

  ballX += ballDX; ballY += ballDY;
  
  if (ballX <= 0 || ballX >= MATRIX_W - 1) ballDX *= -1;
  if (ballY <= 0) ballDY *= -1;

  if (ballY >= MATRIX_H - 1) {
    if (ballX >= paddleX && ballX <= paddleX + 3) {
      ballDY *= -1; ballY = MATRIX_H - 2;
    } else {
      breakoutInit(); return;
    }
  }

  int bx = round(ballX); int by = round(ballY);
  if (by >= 0 && by < 4 && bx >= 0 && bx < 16 && bricks[by][bx]) {
    bricks[by][bx] = false;
    ballDY *= -1;
  }

  for (int y=0; y<4; y++) {
    for (int x=0; x<16; x++) {
      if (bricks[y][x]) drawPixel(x, y, CRGB::Orange);
    }
  }
  for (int i=0; i<3; i++) drawPixel(paddleX + i, MATRIX_H - 1, CRGB::White);
  drawPixel(bx, by, CRGB::Green);
}

// ══════════════════════════════════════════════════════════
// LÓGICA DE CONTROLE E MENU
// ══════════════════════════════════════════════════════════
void drawMenu() {
  bool blink = (millis() % 600) < 300; 

  if (selectedGame != 0 || blink) {
    for(int x = 4; x < 12; x++) drawPixel(x, 3, CRGB::Red);
  }
  if (selectedGame != 1 || blink) {
    for(int x = 4; x < 12; x++) drawPixel(x, 8, CRGB::Blue);
  }
  if (selectedGame != 2 || blink) {
    for(int x = 4; x < 12; x++) drawPixel(x, 13, CRGB::Green);
  }
}

void checkTouch() {
  static unsigned long lastTouch = 0;
  if (millis() - lastTouch < 300) return; 

  bool tUp = touchRead(TOUCH_UP) < TOUCH_THRESH;
  bool tDown = touchRead(TOUCH_DOWN) < TOUCH_THRESH;
  bool tEnter = touchRead(TOUCH_ENTER) < TOUCH_THRESH;

  if (tEnter) {
    lastTouch = millis();
    if (currentState == STATE_MENU) {
      currentState = STATE_PLAYING;
      if (selectedGame == 0) snakeInit();
      else if (selectedGame == 1) tetrisInit();
      else if (selectedGame == 2) breakoutInit();
    } else {
      currentState = STATE_MENU;
    }
  }

  if (currentState == STATE_MENU) {
    if (tUp) {
      lastTouch = millis();
      selectedGame = (selectedGame - 1 + 3) % 3;
    }
    if (tDown) {
      lastTouch = millis();
      selectedGame = (selectedGame + 1) % 3;
    }
  }
}

// ══════════════════════════════════════════════════════════
// SETUP E LOOP
// ══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  randomSeed(analogRead(0));
}

void loop() {
  handleNetwork(); 
  checkTouch();    
  
  FastLED.clear(); 

  if (currentState == STATE_MENU) {
    drawMenu();
  } 
  else if (currentState == STATE_PLAYING) {
    if (selectedGame == 0) snakeLogic();
    else if (selectedGame == 1) tetrisLogic();
    else if (selectedGame == 2) breakoutLogic();
  }

  FastLED.show();
  delay(30); 
}