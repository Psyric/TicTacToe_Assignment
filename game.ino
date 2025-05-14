// Must compile in Arduino with headers files.
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi and MQTT settings
const char* ssid = "my local wifi/device";
const char* password = "my password";

// Replace with your MQTT broker's (GCP instance) address and port:
const char* mqtt_server = "34.30.214.222";
const int mqtt_port = 1883;

// MQTT Topics (used by all scripts)
const char* modeTopic = "tictactoe/mode";
const char* moveTopic = "tictactoe/move";
const char* boardTopic = "tictactoe/board";
const char* resultTopic = "tictactoe/result";
const char* lcdResetTopic = "tictactoe/lcdreset";

LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Game state variables
char board[3][3];
int currentPlayer = 1;  // 1 = Player 1 ('X'), 2 = Player 2 ('O')
int gameMode = 0;       // Set via MQTT on "tictactoe/mode" (1, 2, or 3)
bool modeReceived = false;
bool gameOver = false;

// Variables for received moves (via MQTT)
volatile bool moveReceived = false;
volatile int receivedRow = -1;
volatile int receivedCol = -1;
volatile int receivedPlayer = 0;
String moveHistory = ""; // Logs moves for the current game

// score counters
int scoreP1 = 0;
int scoreP2 = 0;
int draws = 0;
const int TOTAL_GAMES = 100;
int gamesPlayed = 0;

// Timing constants:
unsigned long moveWaitTimeout = 20000; // 20 sec for manual moves (Modes 1 & 2)
unsigned long mode3WaitTimeout = 1000;   // 1 sec for external moves in Mode 3
const unsigned long turnDelay = 100;     // 100 ms between turns
const unsigned long postGameDelay = 1500;  // 1500 ms pause after each game

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
void resetBoard();
String buildBoardString();
void printBoardToSerial();
void publishBoard();
int checkWinner();
bool boardFull();
bool makeMove(int row, int col, int player);
void automatedMove();
void updateLCDScore();
void publishResult(String resultMsg);

void setup_wifi() {
  Wire.begin(14, 13); // I²C pins
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  randomSeed(analogRead(0));
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(modeTopic);
      client.subscribe(moveTopic);
      client.subscribe(lcdResetTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  if (strcmp(topic, modeTopic) == 0) {
    int mode = message.toInt();
    if (mode >= 1 && mode <= 3) {
      gameMode = mode;
      modeReceived = true;
      Serial.print("Game mode selected: ");
      Serial.println(gameMode);
      resetBoard();
      moveHistory = "";
      client.publish(resultTopic, "", true); // Clear any stale result.
      publishBoard();
    }
  }
  else if (strcmp(topic, moveTopic) == 0) {
    if (message.startsWith("P1:") || message.startsWith("P2:")) {
      int msgPlayer = message.startsWith("P1:") ? 1 : 2;
      if (msgPlayer == currentPlayer) { // Process only if it's this player's turn.
        String movePart = message.substring(3);
        int commaIndex = movePart.indexOf(',');
        if (commaIndex != -1) {
          receivedRow = movePart.substring(0, commaIndex).toInt();
          receivedCol = movePart.substring(commaIndex + 1).toInt();
          receivedPlayer = msgPlayer;
          moveReceived = true;
          Serial.print("Received move for Player ");
          Serial.print(receivedPlayer);
          Serial.print(": (");
          Serial.print(receivedRow);
          Serial.print(",");
          Serial.print(receivedCol);
          Serial.println(")");
        }
      }
    }
  }
  else if (strcmp(topic, lcdResetTopic) == 0) {
    if (message.equalsIgnoreCase("reset")) {
      lcd.clear();
      resetBoard();
      publishBoard();
      Serial.println("Reset command received; board and LCD cleared.");
    }
  }
}

void resetBoard() {
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      board[i][j] = ' ';
  moveHistory = "";
  currentPlayer = 1;
}

String buildBoardString() {
  String str = "-------------\n";
  for (int i = 0; i < 3; i++) {
    str += "| ";
    for (int j = 0; j < 3; j++) {
      str += board[i][j];
      str += " | ";
    }
    str += "\n-------------\n";
  }
  return str;
}

void printBoardToSerial() {
  Serial.println(buildBoardString());
}

void publishBoard() {
  String boardStr = buildBoardString();
  client.publish(boardTopic, boardStr.c_str(), true);
  Serial.println("Published board:");
  Serial.println(boardStr);
}

int checkWinner() {
  // Rows
  for (int i = 0; i < 3; i++) {
    if (board[i][0] != ' ' &&
        board[i][0] == board[i][1] &&
        board[i][1] == board[i][2])
      return board[i][0];
  }
  // Columns
  for (int i = 0; i < 3; i++) {
    if (board[0][i] != ' ' &&
        board[0][i] == board[1][i] &&
        board[1][i] == board[2][i])
      return board[0][i];
  }
  // Diagonals
  if (board[0][0] != ' ' &&
      board[0][0] == board[1][1] &&
      board[1][1] == board[2][2])
    return board[0][0];
  if (board[0][2] != ' ' &&
      board[0][2] == board[1][1] &&
      board[1][1] == board[2][0])
    return board[0][2];
  return 0;
}

bool boardFull() {
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      if (board[i][j] == ' ')
        return false;
  return true;
}

bool makeMove(int row, int col, int player) {
  if (row < 0 || row > 2 || col < 0 || col > 2)
    return false;
  if (board[row][col] != ' ')
    return false;
  board[row][col] = (player == 1 ? 'X' : 'O');
  return true;
}

void automatedMove() {
  int freeCount = 0;
  int freeRows[9];
  int freeCols[9];
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (board[i][j] == ' ') {
        freeRows[freeCount] = i;
        freeCols[freeCount] = j;
        freeCount++;
      }
    }
  }
  if (freeCount == 0)
    return;
  int randIndex = random(0, freeCount);
  int row = freeRows[randIndex];
  int col = freeCols[randIndex];
  board[row][col] = (currentPlayer == 1 ? 'X' : 'O');
  Serial.print("Fallback automated move for Player ");
  Serial.print(currentPlayer);
  Serial.print(" at (");
  Serial.print(row);
  Serial.print(",");
  Serial.print(col);
  Serial.println(")");
  moveHistory += "P" + String(currentPlayer) + ":" + String(row) + "," + String(col) + " ";
  Serial.print("Announce: Player ");
  Serial.print(currentPlayer);
  Serial.print(" (Fallback) played at (");
  Serial.print(row);
  Serial.print(",");
  Serial.print(col);
  Serial.println(")");
}

void updateLCDScore() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("P1:");
  lcd.print(scoreP1);
  lcd.print(" P2:");
  lcd.print(scoreP2);
  lcd.setCursor(0, 1);
  lcd.print("Draw:");
  lcd.print(draws);
  Serial.print("LCD Update => P1: ");
  Serial.print(scoreP1);
  Serial.print(" P2: ");
  Serial.print(scoreP2);
  Serial.print(" Draw: ");
  Serial.println(draws);
}

void publishResult(String resultMsg) {
  client.publish(resultTopic, resultMsg.c_str(), false);
  Serial.print("Published result: ");
  Serial.println(resultMsg);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(14, 13);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tic Tac Toe");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();
  Serial.println("Awaiting mode selection (publish on 'tictactoe/mode')...");
  while (!modeReceived) {
    client.loop();
    delay(100);
  }
  Serial.println("Game mode set. Starting gameplay...");
  Serial.print("Mode: ");
  Serial.println(gameMode);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  // Check if 100 games are complete.
  if (gamesPlayed >= TOTAL_GAMES) {
    Serial.println("100 games completed. Final scores:");
    Serial.print("Player 1 wins: ");
    Serial.println(scoreP1);
    Serial.print("Player 2 wins: ");
    Serial.println(scoreP2);
    Serial.print("Draws: ");
    Serial.println(draws);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Final Score:");
    lcd.setCursor(0, 1);
    lcd.print("P1:");
    lcd.print(scoreP1);
    lcd.print(" P2:");
    lcd.print(scoreP2);
    delay(5000);
    lcd.clear();
    while(true) {
      delay(1000);
    }
  }
  // Start a new game:
  resetBoard();
  moveHistory = "";
  gameOver = false;
  currentPlayer = 1;
  client.publish(resultTopic, "", true);
  printBoardToSerial();
  publishBoard();
  int win = 0;
  while (!gameOver) {
    unsigned long startTime = millis();
    moveReceived = false;
    if (gameMode == 1) {
      if (currentPlayer == 1) {
        Serial.println("Waiting for Player 1 move (MQTT) ...");
        while (!moveReceived) {
          client.loop();
          delay(10);
        }
        if (!makeMove(receivedRow, receivedCol, currentPlayer)) {
          Serial.println("Invalid move for P1; fallback applied.");
          automatedMove();
        } else {
          moveHistory += "P1:" + String(receivedRow) + "," + String(receivedCol) + " ";
          Serial.print("Announce: Player 1 played at (");
          Serial.print(receivedRow);
          Serial.print(",");
          Serial.print(receivedCol);
          Serial.println(")");
        }
      } else {
        unsigned long botTimeout = 200;
        Serial.println("Waiting for external Player 2 move (MQTT) ...");
        while (!moveReceived && (millis() - startTime < botTimeout)) {
          client.loop();
          delay(10);
        }
        if (moveReceived) {
          if (!makeMove(receivedRow, receivedCol, currentPlayer)) {
            Serial.println("Invalid move for P2; fallback applied.");
            automatedMove();
          } else {
            moveHistory += "P2:" + String(receivedRow) + "," + String(receivedCol) + " ";
            Serial.print("Announce: Player 2 played at (");
            Serial.print(receivedRow);
            Serial.print(",");
            Serial.print(receivedCol);
            Serial.println(")");
          }
        } else {
          Serial.println("No move received for P2; fallback applied.");
          automatedMove();
        }
        publishBoard();
      }
    }
    else if (gameMode == 2) {
      Serial.print("Waiting for Player ");
      Serial.print(currentPlayer);
      Serial.println(" move (MQTT) ...");
      while (!moveReceived && (millis() - startTime < moveWaitTimeout)) {
        client.loop();
        delay(10);
      }
      if (moveReceived) {
        if (!makeMove(receivedRow, receivedCol, currentPlayer)) {
          Serial.println("Invalid move; fallback applied.");
          automatedMove();
        } else {
          moveHistory += "P" + String(currentPlayer) + ":" + String(receivedRow) + "," + String(receivedCol) + " ";
          Serial.print("Announce: Player ");
          Serial.print(currentPlayer);
          Serial.print(" played at (");
          Serial.print(receivedRow);
          Serial.print(",");
          Serial.print(receivedCol);
          Serial.println(")");
        }
      } else {
        Serial.println("No move received; fallback applied.");
        automatedMove();
      }
      publishBoard();
    }
    else if (gameMode == 3) {
      // Mode 3: Automated mode.
      if (currentPlayer == 1) {
        Serial.println("Arduino bot generating move for P1 ...");
        automatedMove();
        Serial.println("Arduino bot move applied.");
        publishBoard();
      } else {
        Serial.println("Waiting for external Player 2 move (MQTT) ...");
        while (!moveReceived && (millis() - startTime < mode3WaitTimeout)) {
          client.loop();
          delay(10);
        }
        if (moveReceived) {
          if (!makeMove(receivedRow, receivedCol, currentPlayer)) {
            Serial.println("Invalid move for P2; fallback applied.");
            automatedMove();
          } else {
            moveHistory += "P2:" + String(receivedRow) + "," + String(receivedCol) + " ";
            Serial.print("Announce: External bot (P2) played at (");
            Serial.print(receivedRow);
            Serial.print(",");
            Serial.print(receivedCol);
            Serial.println(")");
          }
        } else {
          Serial.println("No move received for P2; fallback applied.");
          automatedMove();
        }
        publishBoard();
      }
    }
    // Always switch players after each turn (for all modes)
    if (!gameOver) {
      currentPlayer = (currentPlayer == 1) ? 2 : 1;
    }
    delay(turnDelay);
    printBoardToSerial();
    win = checkWinner();
    if (win) {
      Serial.print("Player ");
      Serial.print((win == 'X' ? "1" : "2"));
      Serial.println(" wins this game!");
      if (win == 'X')
        scoreP1++;
      else
        scoreP2++;
      gameOver = true;
    } else if (boardFull()) {
      Serial.println("This game is a draw!");
      draws++;
      gameOver = true;
    }
  }
  String resultMsg = "Game " + String(gamesPlayed + 1) + ": ";
  if (win != 0) {
    String winnerStr = (win == 'X' ? "Player 1" : "Player 2");
    int lastSpace = moveHistory.lastIndexOf(" ");
    String winningMove = (lastSpace == -1) ? moveHistory : moveHistory.substring(lastSpace + 1);
    resultMsg += winnerStr + " wins with move " + winningMove;
  } else {
    resultMsg += "Draw";
  }
  publishResult(resultMsg);
  Serial.println(resultMsg);
  Serial.print("Move History: ");
  Serial.println(moveHistory);
  String lcdLine1 = resultMsg.substring(0, (resultMsg.length() < 16 ? resultMsg.length() : 16));
  String lcdLine2 = "";
  if (resultMsg.length() > 16) {
    lcdLine2 = resultMsg.substring(16, (resultMsg.length() < 32 ? resultMsg.length() : 32));
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(lcdLine1);
  lcd.setCursor(0, 1);
  lcd.print(lcdLine2);
  delay(postGameDelay);
  updateLCDScore();
  gamesPlayed++;
  delay(1500);
}



