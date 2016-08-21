#include <Arduino.h>

class Button
{
  public:
    Button(int pin);

    bool exlusivePressed();
  private:
    int _pin;
    int _lastState;
};

// Constructor
Button::Button(int pin)
{
  _pin = pin;
  _lastState = LOW;
  pinMode(_pin, INPUT);
}

bool Button::exlusivePressed() {
  int state = digitalRead(_pin);
  if (state != _lastState) {
    _lastState = state;
    return state == HIGH;
  }
  return false;
}
