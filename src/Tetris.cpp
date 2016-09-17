#include <Arduino.h>
#include "LEDMatrix.cpp"
#include "Button.cpp"
#include <TM1637Display.h>

#define DATA_PIN 5
#define LOAD_PIN 3
#define CLOCK_PIN 4

#define RIGHT_BUTTON_PIN 8
#define LEFT_BUTTON_PIN 9
#define ROTATE_BUTTON_PIN 10
#define SLAM_BUTTON_PIN 7

// Game
void spawnNewShape();
void playDeathAnimation();
void setupNewGame();

// Matrix functions
void renderMatrix(byte *matrix);
void renderShapeInMatrix(byte *original, byte *destination, word shape, int x, int y);
bool collides(byte *matrix, word shape, int x, int y);

// Shape functions
int leftOffsetPositionForShape(word shape);
int rightOffsetPositionForShape(word shape);
int bottomOffsetPositionForShape(word shape);

// Actions
void moveLeftIfPossible();
void moveRightIfPossible();
int removeFullLines(byte *matrix);
void shiftLinesDown(byte *matrix, int toIndex);

byte currentMatrix[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

word shapes[7][4] = {{0xCC00, 0xCC00, 0xCC00, 0xCC00}, // O
                    {0x4444, 0x0F00, 0x2222, 0x0F00}, // I
                    {0x4E00, 0x4640, 0x0E40, 0x4C40}, // T
                    {0x4460, 0x0E80, 0xC440, 0x2E00}, // L
                    {0x44C0, 0x8E00, 0x6440, 0x0E20}, // J
                    {0x4C80, 0xC600, 0x4C80, 0xC600}, // Z
                    {0x8C40, 0x6C00, 0x8C40, 0x6C00}}; // S

word currentShape;
int currentShapeIndex = 0;
int currentX = 0;
int currentY = 0;
int currentRotation = 0;
int score = 0;

int previousRotateButtonState = LOW;
int previousSlamButtonState = LOW;

unsigned long previousMillis = 0;
long gameInterval = 250;

LEDMatrix ledMatrix(2, CLOCK_PIN, DATA_PIN, LOAD_PIN);
TM1637Display display(12, 13);

Button rightButton(RIGHT_BUTTON_PIN);
Button leftButton(LEFT_BUTTON_PIN);
Button rotateButton(ROTATE_BUTTON_PIN);
Button slamButton(SLAM_BUTTON_PIN);

void setup() {
  randomSeed(analogRead(0));
  display.setBrightness(0x0f);

  setupNewGame();
  playDeathAnimation();
  spawnNewShape();

  // NOTE: For some reason, if I omit this line, the game will restart
  // every time a shape (except for "I") hits the floor.
  // If anyone could tell me why, that'd be great...
  Serial.begin(9600);
}

void loop() {
  byte matrix[16];
  renderShapeInMatrix(currentMatrix, matrix, currentShape, currentX, currentY);
  renderMatrix(matrix);

  if (leftButton.exlusivePressed()) {
    moveLeftIfPossible();
  }

  if (rightButton.exlusivePressed()) {
    moveRightIfPossible();
  }

  if (rotateButton.exlusivePressed()) {
    int nextRotation = currentRotation + 1;
    if (nextRotation > 3) {
      nextRotation = 0;
    }
    word nextShape = shapes[currentShapeIndex][nextRotation];
    if (!collides(currentMatrix, nextShape, currentX, currentY)) {
      currentRotation = nextRotation;
      currentShape = nextShape;
    }
  }

  if (slamButton.exlusivePressed()) {
    int bottomOffset = bottomOffsetPositionForShape(currentShape);
    while (!collides(currentMatrix, currentShape, currentX, currentY + 1) && currentY + 4 - bottomOffset <= 15) {
      currentY++;
    }
    currentY--;
  }

  // Increment currentY with given intervals
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= gameInterval) {
    previousMillis = currentMillis;

    int bottomOffset = bottomOffsetPositionForShape(currentShape);
    if (collides(currentMatrix, currentShape, currentX, currentY + 1) || currentY + 4 - bottomOffset > 15) {

      // Check for game over
      if (currentY < 0) {
        playDeathAnimation();
        setupNewGame();
        return;
      }

      renderShapeInMatrix(currentMatrix, currentMatrix, currentShape, currentX, currentY);
      spawnNewShape();
      int linesRemoved = removeFullLines(currentMatrix);
      score += linesRemoved;

      // Increase the game speed
      gameInterval -= linesRemoved * 3;

      display.showNumberDec(score);
    } else {
      currentY++;
    }
  }
}

// ---- Game functions ----

void setupNewGame() {
  currentShapeIndex = 0;
  currentX = 0;
  currentY = 0;
  currentRotation = 0;
  score = 0;
  gameInterval = 250;
  for (int i = 0; i < 16; i++) {
    currentMatrix[i] = 0x0;
  }
  display.showNumberDec(score);
  spawnNewShape();
}

void spawnNewShape() {
  currentShapeIndex = random(7);
  currentShape = shapes[currentShapeIndex][currentRotation];
  currentY = -4;
  currentX = 3;
}

void playDeathAnimation() {
  for (int i = 0; i < 16; i++) {
    if (i < 8) {
      ledMatrix.WriteOne(1, i + 1, 0xff);
    } else {
      ledMatrix.WriteOne(2, i + 1 - 8, 0xff);
    }
    delay(75);
  }
  for (int i = 0; i < 16; i++) {
    if (i < 8) {
      ledMatrix.WriteOne(1, i + 1, 0x00);
    } else {
      ledMatrix.WriteOne(2, i + 1 - 8, 0x00);
    }
    delay(75);
  }
}

// ---- Matrix functions ----

// Returns the numer of lines removed
int removeFullLines(byte *matrix) {
  int res = 0;
  for (int i = 0; i < 16; i++) {
    if (matrix[i] == 0xff) { // Full line
      matrix[i] = 0x0;
      shiftLinesDown(matrix, i);
      res++;
    }
  }
  return res;
}

void shiftLinesDown(byte *matrix, int toIndex) {
  matrix[0] = 0x0;
  for (int i = toIndex; i > 0; i--) {
    matrix[i] = matrix[i - 1];
  }
}

bool collides(byte *matrix, word shape, int x, int y) {
  for (int i = 0; i < 4; i++) {
    if (y + i < 0) { continue; }
    int shift = (3 - i) * 4;
    byte rowsValue = (shape & (0xf << shift)) >> shift;

    // Move to left hand side
    int lOffset = leftOffsetPositionForShape(shape);
    rowsValue = rowsValue << (4 + lOffset);

    // Move to right according to x and offset
    rowsValue = rowsValue >> (x + lOffset);

    if ((matrix[y + i] & rowsValue) > 0) {
      return true;
    }
  }

  return false;
}

void renderMatrix(byte *matrix) {
  for (int i = 0; i < 16; i++) {
    if (i < 8) {
      ledMatrix.WriteOne(1, i + 1, matrix[i]);
    } else {
      ledMatrix.WriteOne(2, i + 1 - 8, matrix[i]);
    }
  }
}

// Render a shape in a Matrix
void renderShapeInMatrix(byte *original, byte *destination, word shape, int x, int y) {
  for (int i = 0; i < 16; i++) {
    destination[i] = original[i];
  }

  for (int i = 0; i < 4; i++) {
    if (y + i < 0) { // Allow for negative Y (to render half a shape)
      continue;
    }

    int shift = (3 - i) * 4;
    byte rowsValue = (shape & (0xf << shift)) >> shift;

    // Move to left hand side
    int lOffset = leftOffsetPositionForShape(shape);
    rowsValue = rowsValue << (4 + lOffset);

    // Move to right according to x and offset
    rowsValue = rowsValue >> (x + lOffset);

    destination[y + i] = original[y + i] | rowsValue;
  }
}

// --- Actions ----

void moveLeftIfPossible() {
  // Check that currentX + bitIndex - 1 >= 0
    int offset = leftOffsetPositionForShape(currentShape);
    bool wouldCollide = collides(currentMatrix, currentShape, currentX - 1, currentY);
    if (currentX + offset - 1 >= 0 && !wouldCollide) {
        currentX--;
    }
}

void moveRightIfPossible() {
  // Check that currentX + bitIndex + 1 < board_width (8)
  int offset = rightOffsetPositionForShape(currentShape);
  bool wouldCollide = collides(currentMatrix, currentShape, currentX + 1, currentY);
  if (currentX + (4 - offset) < 8 && !wouldCollide) {
        currentX++;
    }
}

// --- Shape functions ----

// TODO: Would be great if I could refactor these functions into something
// a little more generic.

// Find leftmost active bit in shape
int leftOffsetPositionForShape(word shape) {
  int offset = 4;
  for (int i = 0; i < 4; i++) {
    int shift = (3 - i) * 4;
    byte rowsValue = (shape & (0xf << shift)) >> shift;
    byte test = 0b1000;
    for (int m = 0; m < 3; m++) {
      int l = rowsValue & test;
      if (l > 0 && m < offset) {
        offset = m;
      }
      test >>= 1;
    }
  }
  return offset;
}

// Find rightmost active bit in shape
int rightOffsetPositionForShape(word shape) {
  int offset = 4;
  for (int i = 0; i < 4; i++) {
    int shift = (3 - i) * 4;
    byte rowsValue = (shape & (0xf << shift)) >> shift;
    byte test = 1;
    for (int m = 0; m < 3; m++) {
      int l = rowsValue & test;
      if (l > 0 && m < offset) {
        offset = m;
      }
      test <<= 1;
    }
  }
  return offset;
}

// Find offset for bottom active bit in shape
int bottomOffsetPositionForShape(word shape) {
  for (int i = 3; i >= 0; i--) {
    int shift = (3 - i) * 4;
    byte rowsValue = (shape & (0xf << shift)) >> shift;
    if (rowsValue > 0) {
      return 3 - i;
    }
  }
}
