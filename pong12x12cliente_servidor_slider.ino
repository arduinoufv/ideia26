#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>

// WiFi Settings
#define WIFI_SSID "Pong_Game"
#define WIFI_PASSWORD "12345678"

// LED Panel Settings
#define LED_PIN D3
#define LED_COUNT 144  // 12x12 = 144 LEDs
#define LED_WIDTH 12
#define LED_HEIGHT 12

// Game Settings
#define REFRESH_RATE 200  // ms between frames - ball speed (higher = slower)

// Colors
#define COLOR_PLAYER1    0xFF0000  // Red
#define COLOR_PLAYER2    0x0000FF  // Blue
#define COLOR_BALL       0xFFFFFF  // White
#define COLOR_EMPTY      0x000000  // Black/off

ESP8266WebServer server(80);
Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Game state
bool gameOver = false;
bool gameStarted = false;
int scorePlayer1 = 0;
int scorePlayer2 = 0;
unsigned long speedBoost = 0; // Temporary speed boost counter

// Game elements
struct Paddle {
  int pos;         // Position (0-9 for a paddle of length 3)
  int score;
};

struct Ball {
  int x, y;
  int dx, dy;  // Direction of movement
};

Paddle player1; // Left player (red)
Paddle player2; // Right player (blue)
Ball ball;

const int PADDLE_LENGTH = 3; // Length of each paddle
const int WIN_SCORE = 5;     // Score needed to win

// Converts XY coordinates to LED index, considering zigzag mapping
int xyToLed(int x, int y) {
  if (y >= LED_HEIGHT || x >= LED_WIDTH || x < 0 || y < 0) {
    return -1;  // Out of bounds
  }
  
  int index;
  if (y % 2 == 0) {
    // Even rows: left to right
    index = y * LED_WIDTH + x;
  } else {
    // Odd rows: right to left
    index = y * LED_WIDTH + (LED_WIDTH - 1 - x);
  }
  
  return index;
}

// Initialize the game
void resetGame() {
  // Initialize player paddles
  player1.pos = LED_HEIGHT / 2 - PADDLE_LENGTH / 2;
  player1.score = 0;
  
  player2.pos = LED_HEIGHT / 2 - PADDLE_LENGTH / 2;
  player2.score = 0;
  
  // Initialize ball in center with random direction
  ball.x = LED_WIDTH / 2;
  ball.y = LED_HEIGHT / 2;
  
  // Random direction, but ensure it's not too vertical or horizontal
  do {
    ball.dx = random(0, 3) - 1; // -1, 0, or 1
    ball.dy = random(0, 3) - 1; // -1, 0, or 1
  } while (ball.dx == 0 || ball.dy == 0); // Ensure ball is moving diagonally
  
  gameOver = false;
  gameStarted = false;
  speedBoost = 0;
}

// Draw the game state on the LED matrix
void drawGame() {
  pixels.clear();
  
  // Draw player 1 paddle (left side, red)
  for (int i = 0; i < PADDLE_LENGTH; i++) {
    if (player1.pos + i >= 0 && player1.pos + i < LED_HEIGHT) {
      int ledIndex = xyToLed(0, player1.pos + i);
      if (ledIndex >= 0) {
        pixels.setPixelColor(ledIndex, COLOR_PLAYER1);
      }
    }
  }
  
  // Draw player 2 paddle (right side, blue)
  for (int i = 0; i < PADDLE_LENGTH; i++) {
    if (player2.pos + i >= 0 && player2.pos + i < LED_HEIGHT) {
      int ledIndex = xyToLed(LED_WIDTH - 1, player2.pos + i);
      if (ledIndex >= 0) {
        pixels.setPixelColor(ledIndex, COLOR_PLAYER2);
      }
    }
  }
  
  // Draw ball
  int ballLedIndex = xyToLed(ball.x, ball.y);
  if (ballLedIndex >= 0) {
    pixels.setPixelColor(ballLedIndex, COLOR_BALL);
  }
  
  pixels.show();
}

// Update the game logic
void updateGame() {
  if (gameOver || !gameStarted) {
    return;
  }
  
  // Move the ball
  ball.x += ball.dx;
  ball.y += ball.dy;
  
  // Ball hit top or bottom wall
  if (ball.y < 0 || ball.y >= LED_HEIGHT) {
    ball.dy = -ball.dy; // Reverse vertical direction
    ball.y = constrain(ball.y, 0, LED_HEIGHT - 1); // Keep ball in bounds
  }
  
  // Check if ball hit player 1 paddle (left side)
  if (ball.x == 1 && ball.y >= player1.pos && ball.y < player1.pos + PADDLE_LENGTH) {
    ball.dx = -ball.dx; // Reverse horizontal direction
    speedBoost = 3;     // Brief speed boost when paddle is hit
  }
  
  // Check if ball hit player 2 paddle (right side)
  if (ball.x == LED_WIDTH - 2 && ball.y >= player2.pos && ball.y < player2.pos + PADDLE_LENGTH) {
    ball.dx = -ball.dx; // Reverse horizontal direction
    speedBoost = 3;     // Brief speed boost when paddle is hit
  }
  
  // Ball went past player 1 (left side)
  if (ball.x < 0) {
    player2.score++;
    
    if (player2.score >= WIN_SCORE) {
      gameOver = true;
    } else {
      // Reset ball position, keep scores
      ball.x = LED_WIDTH / 2;
      ball.y = LED_HEIGHT / 2;
      ball.dx = -1; // Start toward player 1
      ball.dy = random(0, 3) - 1; // Random vertical direction
      if (ball.dy == 0) ball.dy = 1; // Ensure some vertical movement
      gameStarted = false; // Pause briefly before continuing
    }
  }
  
  // Ball went past player 2 (right side)
  if (ball.x >= LED_WIDTH) {
    player1.score++;
    
    if (player1.score >= WIN_SCORE) {
      gameOver = true;
    } else {
      // Reset ball position, keep scores
      ball.x = LED_WIDTH / 2;
      ball.y = LED_HEIGHT / 2;
      ball.dx = 1; // Start toward player 2
      ball.dy = random(0, 3) - 1; // Random vertical direction
      if (ball.dy == 0) ball.dy = 1; // Ensure some vertical movement
      gameStarted = false; // Pause briefly before continuing
    }
  }
}

// Embedded HTML page with sliders instead of buttons
const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>LED Pong Game</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 0;
      padding: 15px;
      background-color: #f0f0f0;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    .game-info {
      display: flex;
      justify-content: space-between;
      margin-bottom: 20px;
    }
    .player {
      flex: 1;
      padding: 10px;
      border-radius: 5px;
    }
    .player1 {
      background-color: #ffeeee;
      margin-right: 10px;
    }
    .player2 {
      background-color: #eeeeff;
      margin-left: 10px;
    }
    .score {
      font-size: 32px;
      font-weight: bold;
    }
    .status {
      font-size: 24px;
      font-weight: bold;
      margin: 10px 0;
    }
    .controls {
      display: flex;
      justify-content: space-between;
      margin-top: 20px;
    }
    .player-controls {
      flex: 1;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 0 10px;
    }
    .vertical-slider-container {
      height: 200px;
      display: flex;
      align-items: center;
      justify-content: center;
      position: relative;
    }
    .vertical-slider {
      -webkit-appearance: slider-vertical;
      appearance: slider-vertical;
      width: 50px;
      height: 200px;
      transform: rotate(180deg);
    }
    /* For browsers that don't support -webkit-appearance: slider-vertical */
    @supports not (-webkit-appearance: slider-vertical) {
      .vertical-slider {
        width: 200px;
        height: 50px;
        transform: rotate(270deg) translateX(-75px) translateY(-75px);
        position: absolute;
      }
    }
    .player1-controls .vertical-slider {
      accent-color: #ff4444;
    }
    .player2-controls .vertical-slider {
      accent-color: #4444ff;
    }
    .btn {
      border: none;
      padding: 15px 25px;
      margin: 5px;
      font-size: 18px;
      border-radius: 5px;
      cursor: pointer;
      user-select: none;
      touch-action: manipulation;
      width: 80%;
    }
    .start-btn {
      background-color: #4CAF50;
      color: white;
      font-size: 20px;
      padding: 15px 30px;
      margin: 20px auto;
      display: block;
    }
    .reset-btn {
      background-color: #f44336;
      color: white;
      font-size: 20px;
      padding: 15px 30px;
      margin: 20px auto;
      display: block;
    }
    .instructions {
      margin-top: 20px;
      padding: 10px;
      background: #e8f5e9;
      border-radius: 5px;
      font-size: 14px;
      text-align: left;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>LED Pong Game</h1>
    
    <div class="game-info">
      <div class="player player1">
        <h2>Player 1 (Red)</h2>
        <div class="score" id="score1">0</div>
      </div>
      
      <div class="player player2">
        <h2>Player 2 (Blue)</h2>
        <div class="score" id="score2">0</div>
      </div>
    </div>
    
    <div class="status" id="gameStatus">Ready to Play</div>
    
    <button class="start-btn" id="startBtn">START GAME</button>
    <button class="reset-btn" id="resetBtn">RESET GAME</button>
    
    <div class="controls">
      <div class="player-controls player1-controls">
        <h3>Player 1 Controls</h3>
        <div class="vertical-slider-container">
          <input type="range" min="0" max="9" value="4" class="vertical-slider" id="p1Slider">
        </div>
      </div>
      
      <div class="player-controls player2-controls">
        <h3>Player 2 Controls</h3>
        <div class="vertical-slider-container">
          <input type="range" min="0" max="9" value="4" class="vertical-slider" id="p2Slider">
        </div>
      </div>
    </div>
    
    <div class="instructions">
      <p><strong>How to play:</strong></p>
      <ul>
        <li>Each player controls a paddle (slider) on one side of the screen</li>
        <li>Player 1 (Red) is on the left side</li>
        <li>Player 2 (Blue) is on the right side</li>
        <li>Move the sliders up and down to position your paddle</li>
        <li>Try to hit the ball with your paddle</li>
        <li>Score a point if your opponent misses the ball</li>
        <li>First player to reach 5 points wins!</li>
      </ul>
    </div>
  </div>
  
  <script>
    // DOM elements
    const startBtn = document.getElementById('startBtn');
    const resetBtn = document.getElementById('resetBtn');
    const gameStatus = document.getElementById('gameStatus');
    const score1Display = document.getElementById('score1');
    const score2Display = document.getElementById('score2');
    
    const p1Slider = document.getElementById('p1Slider');
    const p2Slider = document.getElementById('p2Slider');
    
    // Player position tracking
    let p1LastPos = 4;
    let p2LastPos = 4;
    
    // Event listeners for sliders
    p1Slider.addEventListener('input', () => {
      const newPos = parseInt(p1Slider.value);
      if (newPos !== p1LastPos) {
        p1LastPos = newPos;
        setPaddlePosition(1, newPos);
      }
    });
    
    p2Slider.addEventListener('input', () => {
      const newPos = parseInt(p2Slider.value);
      if (newPos !== p2LastPos) {
        p2LastPos = newPos;
        setPaddlePosition(2, newPos);
      }
    });
    
    // For touch devices, make sure slider values update when touch ends
    p1Slider.addEventListener('touchend', () => {
      setPaddlePosition(1, parseInt(p1Slider.value));
    });
    
    p2Slider.addEventListener('touchend', () => {
      setPaddlePosition(2, parseInt(p2Slider.value));
    });
    
    // Keyboard support
    document.addEventListener('keydown', (e) => {
      switch(e.key) {
        case 'w':
        case 'W':
          if (p1LastPos > 0) {
            p1LastPos--;
            p1Slider.value = p1LastPos;
            setPaddlePosition(1, p1LastPos);
          }
          e.preventDefault();
          break;
        case 's':
        case 'S':
          if (p1LastPos < 9) {
            p1LastPos++;
            p1Slider.value = p1LastPos;
            setPaddlePosition(1, p1LastPos);
          }
          e.preventDefault();
          break;
        case 'ArrowUp':
          if (p2LastPos > 0) {
            p2LastPos--;
            p2Slider.value = p2LastPos;
            setPaddlePosition(2, p2LastPos);
          }
          e.preventDefault();
          break;
        case 'ArrowDown':
          if (p2LastPos < 9) {
            p2LastPos++;
            p2Slider.value = p2LastPos;
            setPaddlePosition(2, p2LastPos);
          }
          e.preventDefault();
          break;
      }
    });
    
    // Function to set paddle position
    function setPaddlePosition(player, position) {
      fetch(`/setpaddle?player=${player}&pos=${position}`)
        .catch(e => console.log('Error: ' + e));
    }
    
    // Start game function
    startBtn.addEventListener('click', () => {
      fetch('/start')
        .then(() => {
          gameStatus.textContent = "Game Started";
          gameStatus.style.color = "green";
        })
        .catch(e => console.log('Error: ' + e));
    });
    
    // Reset game function
    resetBtn.addEventListener('click', () => {
      fetch('/reset')
        .then(() => {
          gameStatus.textContent = "Ready to Play";
          gameStatus.style.color = "black";
          score1Display.textContent = "0";
          score2Display.textContent = "0";
          // Reset sliders to middle position
          p1Slider.value = 4;
          p2Slider.value = 4;
          p1LastPos = 4;
          p2LastPos = 4;
        })
        .catch(e => console.log('Error: ' + e));
    });
    
    // Function to update game status
    function updateStatus() {
      fetch('/status')
        .then(r => r.json())
        .then(data => {
          score1Display.textContent = data.score1;
          score2Display.textContent = data.score2;
          
          if (data.gameOver) {
            if (data.score1 > data.score2) {
              gameStatus.textContent = "PLAYER 1 WINS!";
              gameStatus.style.color = "red";
            } else {
              gameStatus.textContent = "PLAYER 2 WINS!";
              gameStatus.style.color = "blue";
            }
          } else if (data.gameStarted) {
            gameStatus.textContent = "Game In Progress";
            gameStatus.style.color = "green";
          } else {
            gameStatus.textContent = "Ready to Play";
            gameStatus.style.color = "black";
          }
        })
        .catch(e => console.log('Error: ' + e));
    }
    
    // Update status every second
    setInterval(updateStatus, 1000);
    
    // Initial status check
    updateStatus();
  </script>
</body>
</html>
)=====";

// Root page route
void handleRoot() {
  server.send(200, "text/html", MAIN_page);
}

// Route to set paddle position directly
void handleSetPaddle() {
  if (server.hasArg("player") && server.hasArg("pos")) {
    int player = server.arg("player").toInt();
    int position = server.arg("pos").toInt();
    
    // Ensure position is within valid range
    position = constrain(position, 0, LED_HEIGHT - PADDLE_LENGTH);
    
    if (player == 1) {
      player1.pos = position;
    } else if (player == 2) {
      player2.pos = position;
    }
    
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// Route to start the game
void handleStart() {
  gameStarted = true;
  server.send(200, "text/plain", "Game Started");
}

// Route to reset the game
void handleReset() {
  resetGame();
  server.send(200, "text/plain", "Game Reset");
}

// Route to get game status
void handleStatus() {
  String status = "{\"score1\":" + String(player1.score) + 
                  ",\"score2\":" + String(player2.score) + 
                  ",\"gameOver\":" + String(gameOver ? "true" : "false") +
                  ",\"gameStarted\":" + String(gameStarted ? "true" : "false") + "}";
  server.send(200, "application/json", status);
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0)); // Initialize random number generator
  
  // Initialize LED strip
  pixels.begin();
  pixels.clear();
  pixels.show();
  
  // Configure WiFi access point
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("WiFi Access Point started");
  Serial.print("IP Address: ");
  Serial.println(IP);
  
  // Configure server routes
  server.on("/", handleRoot);
  server.on("/setpaddle", handleSetPaddle); // New endpoint for slider control
  server.on("/start", handleStart);
  server.on("/reset", handleReset);
  server.on("/status", handleStatus);
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  
  // Initialize game
  resetGame();
  
  // Initial animation
  // Red to Blue fade across the display
  for (int i = 0; i < LED_COUNT; i++) {
    int row = i / LED_WIDTH;
    int col = i % LED_WIDTH;
    
    // Calculate color (fade from red to blue)
    int r = map(col, 0, LED_WIDTH-1, 255, 0);
    int b = map(col, 0, LED_WIDTH-1, 0, 255);
    
    pixels.setPixelColor(xyToLed(col, row), pixels.Color(r, 0, b));
    
    if ((i % LED_WIDTH) == LED_WIDTH-1) {
      pixels.show();
      delay(50); // Slow down animation to show row by row
    }
  }
  delay(500);
  pixels.clear();
  pixels.show();
}

unsigned long lastUpdateTime = 0;
unsigned long lastBlinkTime = 0;
bool blinkState = false;

void loop() {
  server.handleClient();
  
  unsigned long currentTime = millis();
  unsigned long updateInterval = REFRESH_RATE;
  
  // Apply speed boost if active
  if (speedBoost > 0) {
    updateInterval = REFRESH_RATE * 0.7; // 30% faster when boosted
    if (currentTime - lastUpdateTime >= 100) { // Check every 100ms if boost should end
      speedBoost--;
    }
  }
  
  // Update game state
  if (currentTime - lastUpdateTime >= updateInterval) {
    if (gameStarted && !gameOver) {
      updateGame();
    }
    
    // If waiting to start or game over, blink the ball
    if (!gameStarted || gameOver) {
      if (currentTime - lastBlinkTime >= 500) { // Blink every 500ms
        blinkState = !blinkState;
        lastBlinkTime = currentTime;
      }
      
      // Draw with or without ball depending on blink state
      drawGame();
      
      // If blinking off, clear the ball
      if (!blinkState) {
        int ballLedIndex = xyToLed(ball.x, ball.y);
        if (ballLedIndex >= 0) {
          pixels.setPixelColor(ballLedIndex, COLOR_EMPTY);
          pixels.show();
        }
      }
    } else {
      // Normal game state, always draw
      drawGame();
    }
    
    lastUpdateTime = currentTime;
  }
}