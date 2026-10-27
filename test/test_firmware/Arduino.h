#pragma once

// Host-side stand-in for the Arduino core, just big enough for this firmware.
// Everything the firmware does to the "hardware" is recorded by hw_sim.h.

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <string>

#define PROGMEM
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define DEC 10

constexpr uint8_t A1 = 15;
constexpr uint8_t A2 = 16;

#define constrain(amount, low, high) \
  ((amount) < (low) ? (low) : ((amount) > (high) ? (high) : (amount)))

template <typename T>
T min(T a, T b) { return a < b ? a : b; }

template <typename T>
T max(T a, T b) { return a > b ? a : b; }

// F("text") yields a distinct pointer type, like the AVR flash-string helper.
class __FlashStringHelper;
#define F(text) (reinterpret_cast<const __FlashStringHelper *>(text))

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t level);
int digitalRead(uint8_t pin);
void delay(unsigned long milliseconds);
void delayMicroseconds(unsigned int microseconds);
unsigned long millis();

// Same overload set as Arduino's Print so integer widths print identically.
class Print {
public:
  virtual ~Print() {}

  virtual size_t write(uint8_t character) = 0;

  size_t print(const __FlashStringHelper *text);
  size_t print(const char *text);
  size_t print(char character);
  size_t print(unsigned char value, int base = DEC);
  size_t print(int value, int base = DEC);
  size_t print(unsigned int value, int base = DEC);
  size_t print(long value, int base = DEC);
  size_t print(unsigned long value, int base = DEC);
  size_t print(double value, int digits = 2);

  size_t println();
  size_t println(const __FlashStringHelper *text);
  size_t println(const char *text);
  size_t println(char character);
  size_t println(unsigned char value, int base = DEC);
  size_t println(int value, int base = DEC);
  size_t println(unsigned int value, int base = DEC);
  size_t println(long value, int base = DEC);
  size_t println(unsigned long value, int base = DEC);
  size_t println(double value, int digits = 2);

private:
  size_t writeText(const std::string &text);
};

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
};

// Scripted serial port: tests feed() input and read output().
class SerialPort : public Stream {
public:
  void begin(unsigned long baud);

  size_t write(uint8_t character) override;
  int available() override;
  int read() override;
  int peek() override;

  void feed(const std::string &text);
  const std::string &output() const;
  void clearOutput();
  void reset();
  unsigned long baud() const;

private:
  std::string outputText;
  std::deque<char> inputBytes;
  unsigned long baudRate = 0;
};

extern SerialPort Serial;
