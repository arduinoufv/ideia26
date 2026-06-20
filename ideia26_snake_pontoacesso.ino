#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>

/* ===================== CONFIG ===================== */
#define MATRIX_W 16
#define MATRIX_H 16
#define NUM_LEDS 256

#define LED_PIN D3   // GPIO0
#define BRIGHTNESS 80

const char* ssid = "SnakeESP";
const char* password = "12345678";
/* ================================================== */

ESP8266WebServer server(80);
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

/* ===================== SNAKE ===================== */
struct Point {
  int8_t x;
  int8_t y;
};

#define MAX_SNAKE 256
Point snake[MAX_SNAKE];
uint8_t snakeLen = 1;

Point dir = {1, 0};
Point food;

bool gameOver = false;
unsigned long lastMove = 0;
#define GAME_SPEED 200
/* ================================================ */

uint16_t getIndex(uint8_t x, uint8_t y) {
  // serpentina
  if (y % 2 == 0) return y * MATRIX_W + x;
  return y * MATRIX_W + (MATRIX_W - 1 - x);
}

/* ===================== WEB ===================== */
void handleRoot() {
  server.send(200, "text/html",
    "<!DOCTYPE html><html>"
    "<head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>"
    "body{background:#111;color:#fff;font-family:Arial;text-align:center;}"
    "h2{margin-top:10px;}"
    ".btn{"
    " width:120px;height:120px;"
    " font-size:24px;"
    " margin:10px;"
    " border:none;"
    " border-radius:20px;"
    " background:#2ecc71;"
    " color:#000;"
    " box-shadow:0 6px #1e8449;"
    "}"
    ".btn:active{transform:translateY(4px);box-shadow:0 2px #1e8449;}"
    ".row{display:flex;justify-content:center;}"
    ".reset{background:#e74c3c;box-shadow:0 6px #922b21;color:#fff;}"
    "</style>"
    "</head>"
    "<body>"
    "<h2>Snake ESP8266</h2>"

    "<div class='row'>"
    "<form action='/up'><button class='btn'>UP</button></form>"
    "</div>"

    "<div class='row'>"
    "<form action='/left'><button class='btn'>LEFT</button></form>"
    "<form action='/right'><button class='btn'>RIGHT</button></form>"
    "</div>"

    "<div class='row'>"
    "<form action='/down'><button class='btn'>DOWN</button></form>"
    "</div>"

    "<br>"
    "<form action='/reset'><button class='btn reset'>RESET</button></form>"

    "</body></html>"
  );
}

void setDir(int8_t x, int8_t y) {
  if (dir.x + x == 0 && dir.y + y == 0) return;
  dir.x = x;
  dir.y = y;
}

void setupServer() {
  server.on("/", handleRoot);
  server.on("/up",    [](){ setDir(0,-1); server.send(204,""); });
  server.on("/down",  [](){ setDir(0, 1); server.send(204,""); });
  server.on("/left",  [](){ setDir(1,0); server.send(204,""); });
  server.on("/right", [](){ setDir(-1, 0); server.send(204,""); });
  server.on("/reset", [](){ resetGame(); server.send(204,""); });
  server.begin();
}
/* ================================================ */

void resetGame() {
  snakeLen = 1;
  snake[0] = {8, 8};
  dir = {1, 0};
  gameOver = false;
  randomSeed(millis());
  generateFood();
}

void generateFood() {
  bool ok;
  do {
    ok = true;
    food.x = random(MATRIX_W);
    food.y = random(MATRIX_H);
    for (uint8_t i = 0; i < snakeLen; i++)
      if (snake[i].x == food.x && snake[i].y == food.y)
        ok = false;
  } while (!ok);
}

void updateGame() {
  Point head = snake[0];
  head.x = (head.x + dir.x + MATRIX_W) % MATRIX_W;
  head.y = (head.y + dir.y + MATRIX_H) % MATRIX_H;

  for (uint8_t i = 0; i < snakeLen; i++) {
    if (snake[i].x == head.x && snake[i].y == head.y) {
      gameOver = true;
      return;
    }
  }

  for (int i = snakeLen; i > 0; i--)
    snake[i] = snake[i-1];
  snake[0] = head;

  if (head.x == food.x && head.y == food.y) {
    if (snakeLen < MAX_SNAKE) snakeLen++;
    generateFood();
  }
}

void drawGame() {
  strip.clear();

  // food
  strip.setPixelColor(getIndex(food.x, food.y),
                      strip.Color(255,0,0));

  // snake
  for (uint8_t i = 0; i < snakeLen; i++) {
    uint32_t c = (i == 0)
      ? strip.Color(0,255,0)
      : strip.Color(0,120,0);
    strip.setPixelColor(getIndex(snake[i].x, snake[i].y), c);
  }

  strip.show();
}

void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  WiFi.softAP(ssid, password);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  setupServer();
  resetGame();
}

void loop() {
  server.handleClient();

  if (!gameOver && millis() - lastMove > GAME_SPEED) {
    lastMove = millis();
    updateGame();
    drawGame();
  }
}
