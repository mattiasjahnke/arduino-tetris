#include <Arduino.h>
#include "LEDMatrix.cpp"
#include <TM1637Display.h>

#define DATA_PIN 2
#define LOAD_PIN 3
#define CLOCK_PIN 4

#define LEFT_BUTTON_PIN 9
#define RIGHT_BUTTON_PIN 8
#define ROTATE_BUTTON_PIN 10
#define SLAM_BUTTON_PIN 11

void spawnNewShape();
void renderGrid(byte *grid);
void moveLeftIfPossible();
void moveRightIfPossible();
int leftOffsetPositionForShape(word shape);
int rightOffsetPositionForShape(word shape);
void renderShapeInArray(byte *original, byte *destination, word shape, int x, int y);
bool collides(byte *grid, word shape, int x, int y);
int removeFullLines(byte *grid);
void shiftLinesDown(byte *grid, int toIndex);
int bottomOffsetPositionForShape(word shape);
void playDeathAnimation();
void setupNewGame();

byte currentGrid[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

word shapes[7][4] = {{0xCC00, 0xCC00, 0xCC00, 0xCC00}, // O
                    {0x4444, 0x0F00, 0x2222, 0x0F00}, // I
                    {0x4E00, 0x4C40, 0x0E40, 0x4640}, // J
                    {0x4460, 0x0E80, 0xC440, 0x2E00}, // L
                    {0x44C0, 0x8E00, 0x6440, 0x0E20}, // T
                    {0x8C40, 0xC600, 0x8C40, 0xC600}, // S
                    {0x8C40, 0x6C00, 0x8C40, 0x6C00}}; // Z

word currentShape;
int currentShapeIndex = 0;
int currentX = 0;
int currentY = 0;
int currentRotation = 0;
int score = 0;

int previousLeftButtonState = LOW;
int previousRightButtonState = LOW;
int previousRotateButtonState = LOW;
int previousSlamButtonState = LOW;

unsigned long previousMillis = 0;
long gameInterval = 250;

LEDMatrix matrix(2, CLOCK_PIN, DATA_PIN, LOAD_PIN);
TM1637Display display(12, 13);

void setup() {
  randomSeed(analogRead(0));

  display.setBrightness(0x0f);
  setupNewGame();
  playDeathAnimation();

  // Buttons
  pinMode(LEFT_BUTTON_PIN, INPUT);
  pinMode(RIGHT_BUTTON_PIN, INPUT);
  pinMode(ROTATE_BUTTON_PIN, INPUT);
  pinMode(SLAM_BUTTON_PIN, INPUT);

  Serial.begin(9600);

  spawnNewShape();
}

void loop() {
  byte grid[16];
  renderShapeInArray(currentGrid, grid, currentShape, currentX, currentY);
  renderGrid(grid);

  // Refactor button handling
  int leftButtonState = digitalRead(LEFT_BUTTON_PIN);
  if (leftButtonState != previousLeftButtonState) {
    if (leftButtonState == HIGH) {
      moveLeftIfPossible();
    }
    previousLeftButtonState = leftButtonState;
  }

  int rightButtonState = digitalRead(RIGHT_BUTTON_PIN);
  if (rightButtonState != previousRightButtonState) {
    if (rightButtonState == HIGH) {
      moveRightIfPossible();
    }
    previousRightButtonState = rightButtonState;
  }

  int rotateButtonState = digitalRead(ROTATE_BUTTON_PIN);
  if (rotateButtonState != previousRotateButtonState) {
    if (rotateButtonState == HIGH) {
      if (++currentRotation > 3) {
        currentRotation = 0;
      }
      currentShape = shapes[currentShapeIndex][currentRotation];
    }
    previousRotateButtonState = rotateButtonState;
  }

  bool iterations = 1;
  int slamButtonState = digitalRead(SLAM_BUTTON_PIN);
  if (slamButtonState != previousSlamButtonState) {
    if (slamButtonState == HIGH) {
      iterations = 16;
    }
    previousSlamButtonState = slamButtonState;
  }

  // Increment currentY with given intervals
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= gameInterval) {
    previousMillis = currentMillis;

    int bottomOffset = bottomOffsetPositionForShape(currentShape);
    if (collides(currentGrid, currentShape, currentX, currentY + 1) || currentY + 4 - bottomOffset > 15) {

      // Check for game over
      if (currentY < 0) {
        playDeathAnimation();
        setupNewGame();
        return;
      }

      renderShapeInArray(currentGrid, currentGrid, currentShape, currentX, currentY);
      spawnNewShape();
      int linesRemoved = removeFullLines(currentGrid);
      score += linesRemoved;

      // Increase the game speed
      gameInterval -= linesRemoved * 3;

      display.showNumberDec(score);
    } else {
      currentY++;
    }
  }
}

// Returns the numer of lines removed
int removeFullLines(byte *grid) {
  int res = 0;
  for (int i = 0; i < 16; i++) {
    if (grid[i] == 0xff) { // Full line
      grid[i] = 0x0;
      shiftLinesDown(grid, i);
      res++;
    }
  }
  return res;
}

void shiftLinesDown(byte *grid, int toIndex) {
  grid[0] = 0x0;
  for (int i = toIndex; i > 0; i--) {
    grid[i] = grid[i - 1];
  }
}

void setupNewGame() {
  currentShapeIndex = 0;
  currentX = 0;
  currentY = 0;
  currentRotation = 0;
  score = 0;
  gameInterval = 250;
  for (int i = 0; i < 16; i++) {
    currentGrid[i] = 0x0;
  }
  display.showNumberDec(score);
  spawnNewShape();
}

void playDeathAnimation() {
  for (int i = 0; i < 16; i++) {
    if (i < 8) {
      matrix.WriteOne(1, i + 1, 0xff);
    } else {
      matrix.WriteOne(2, i + 1 - 8, 0xff);
    }
    delay(75);
  }
  for (int i = 0; i < 16; i++) {
    if (i < 8) {
      matrix.WriteOne(1, i + 1, 0x00);
    } else {
      matrix.WriteOne(2, i + 1 - 8, 0x00);
    }
    delay(75);
  }
}

bool collides(byte *grid, word shape, int x, int y) {
  for (int i = 0; i < 4; i++) {
    if (y + i < 0) { continue; }
    int shift = (3 - i) * 4;
    byte rowsValue = (shape & (0xf << shift)) >> shift;

    // Move to left hand side
    int lOffset = leftOffsetPositionForShape(shape);
    rowsValue = rowsValue << (4 + lOffset);

    // Move to right according to x and offset
    rowsValue = rowsValue >> (x + lOffset);

    if ((grid[y + i] & rowsValue) > 0) {
      return true;
    }
  }

  return false;
}

void moveLeftIfPossible() {
  // Check to see that it's possible to go in direction
  // Find leftmost active bit in shape
  // Check that currentX + bitIndex - 1 >= 0
    int offset = leftOffsetPositionForShape(currentShape);
    bool wouldCollide = collides(currentGrid, currentShape, currentX - 1, currentY);
    if (currentX + offset - 1 >= 0 && !wouldCollide) {
        currentX--;
    }
}

void moveRightIfPossible() {
  // Check to see that it's possible to go in direction
  // Find rightmost active bit in shape
  // Check that currentX + bitIndex + 1 < board_width (8)
  int offset = rightOffsetPositionForShape(currentShape);
  bool wouldCollide = collides(currentGrid, currentShape, currentX + 1, currentY);
  if (currentX + (4 - offset) < 8 && !wouldCollide) {
        currentX++;
    }
}

// TODO refactor
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

// TODO refactor
int rightOffsetPositionForShape(word shape) {
  int offset = 4;
  for (int i = 0; i < 4; i++) { // Does this need to be other way?
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

int bottomOffsetPositionForShape(word shape) {
  for (int i = 3; i >= 0; i--) {
    int shift = (3 - i) * 4;
    byte rowsValue = (shape & (0xf << shift)) >> shift;
    if (rowsValue > 0) {
      return 3 - i;
    }
  }
}

void renderGrid(byte *grid) {
  for (int i = 0; i < 16; i++) {
    if (i < 8) {
      matrix.WriteOne(1, i + 1, grid[i]);
    } else {
      matrix.WriteOne(2, i + 1 - 8, grid[i]);
    }
  }
}

void renderShapeInArray(byte *original, byte *destination, word shape, int x, int y) {
  for (int i = 0; i < 16; i++) {
    destination[i] = original[i];
  }

  for (int i = 0; i < 4; i++) {

    // Allow for negative Y (to render half a shape)
    if (y + i < 0) {
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

void spawnNewShape() {
  currentShapeIndex = random(6);
  currentShape = shapes[currentShapeIndex][currentRotation];
  currentY = -4;
  currentX = 3;
}
