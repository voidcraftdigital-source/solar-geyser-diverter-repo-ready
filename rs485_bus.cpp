#include "rs485_bus.h"
#include "pins.h"

namespace {
  HardwareSerial &bus = Serial1;
}

void RS485Bus::begin(uint32_t baud){
  pinMode(Pins::RS485_DE_RE, OUTPUT);
  digitalWrite(Pins::RS485_DE_RE, LOW); // receive by default — safe default per Specification §25
  bus.begin(baud, SERIAL_8N1, Pins::RS485_RX, Pins::RS485_TX);
}

void RS485Bus::write(const uint8_t *data, size_t len){
  digitalWrite(Pins::RS485_DE_RE, HIGH);
  delayMicroseconds(50); // let the transceiver's driver actually enable before the first bit
  bus.write(data, len);
  bus.flush(); // block until physically transmitted, not just buffered
  digitalWrite(Pins::RS485_DE_RE, LOW);
}

int RS485Bus::available(){ return bus.available(); }
int RS485Bus::read(){ return bus.read(); }
void RS485Bus::clearInput(){ while (bus.available()) bus.read(); }

String RS485Bus::runLoopbackTest(){
  clearInput();
  const char *pattern = "RS485-TEST";
  size_t patternLen = strlen(pattern);
  write((const uint8_t *)pattern, patternLen);

  String received;
  uint32_t start = millis();
  while (millis() - start < 200){
    while (bus.available()) received += (char)bus.read();
    if (received.length() >= patternLen) break;
  }

  if (received == pattern) return "PASS -- loopback bytes matched exactly";
  if (received.length() == 0) return "FAIL -- nothing received; check the TX/RX jumper and DE/RE wiring";
  return "FAIL -- received '" + received + "', expected '" + String(pattern) + "'";
}
