#include <Arduino.h>

class LEDMatrix
{
  public:
    LEDMatrix(int numberOfMatrices, int clockPin, int dataPin, int loadPin);

    void WriteAll(byte reg, byte col);
    void WriteOne(byte matrixIndex, byte reg, byte col);
  private:
    // define max7219 registers
    byte max7219_reg_noop        = 0x00;
    byte max7219_reg_digit0      = 0x01;
    byte max7219_reg_digit1      = 0x02;
    byte max7219_reg_digit2      = 0x03;
    byte max7219_reg_digit3      = 0x04;
    byte max7219_reg_digit4      = 0x05;
    byte max7219_reg_digit5      = 0x06;
    byte max7219_reg_digit6      = 0x07;
    byte max7219_reg_digit7      = 0x08;
    byte max7219_reg_decodeMode  = 0x09;
    byte max7219_reg_intensity   = 0x0a;
    byte max7219_reg_scanLimit   = 0x0b;
    byte max7219_reg_shutdown    = 0x0c;
    byte max7219_reg_displayTest = 0x0f;

    int _numberOfMatrices;
    int _clockPin;
    int _dataPin;
    int _loadPin;

    void putByte(byte data);
};

// Constructor
LEDMatrix::LEDMatrix(int numberOfMatrices, int clockPin, int dataPin, int loadPin)
{
  _numberOfMatrices = numberOfMatrices;
  _clockPin = clockPin;
  _dataPin = dataPin;
  _loadPin = loadPin;

  pinMode(_dataPin, OUTPUT);
  pinMode(_clockPin, OUTPUT);
  pinMode(_loadPin, OUTPUT);

  //initiation of the max 7219
  WriteAll(max7219_reg_scanLimit, 0x07);
  WriteAll(max7219_reg_decodeMode, 0x00);  // using an led matrix (not digits)
  WriteAll(max7219_reg_shutdown, 0x01);    // not in shutdown mode
  WriteAll(max7219_reg_displayTest, 0x00); // no display test
  for (int e = 1; e <= 8; e++) { // empty registers, turn all LEDs off
    WriteAll(e, 0);
  }
  // the first 0x0f is the value you can set
  // range: 0x00 to 0x0f
  WriteAll(max7219_reg_intensity, 0x0f & 0x0f);
}

void LEDMatrix::WriteAll(byte reg, byte col)
{
  digitalWrite(_loadPin, LOW);  // begin
  for (int c = 1; c <= _numberOfMatrices; c++) {
    putByte(reg);  // specify register
    putByte(col);//((data & 0x01) * 256) + data >> 1); // put data
  }
  digitalWrite(_loadPin, LOW);
  digitalWrite(_loadPin, HIGH);
}

void LEDMatrix::WriteOne(byte matrixIndex, byte reg, byte col)
{
  digitalWrite(_loadPin, LOW);  // begin

  for (int c = 2; c > matrixIndex; c--) {
    putByte(0);    // means no operation
    putByte(0);    // means no operation
  }

  putByte(reg);     // specify register
  putByte(col);     //((data & 0x01) * 256) + data >> 1); // put data

  for (int c = matrixIndex - 1; c >= 1; c--) {
    putByte(0);    // means no operation
    putByte(0);    // means no operation
  }

  digitalWrite(_loadPin, LOW); // and load the stuff
  digitalWrite(_loadPin, HIGH);
}

void LEDMatrix::putByte(byte data) {
  byte i = 8;
  byte mask;
  while (i > 0) {
    mask = 0x01 << (i - 1);      // get bitmask
    digitalWrite(_clockPin, LOW);   // tick
    if (data & mask) {           // choose bit
      digitalWrite(_dataPin, HIGH);// send 1
    } else {
      digitalWrite(_dataPin, LOW); // send 0
    }
    digitalWrite(_clockPin, HIGH);   // tock
    --i;                         // move to lesser bit
  }
}
