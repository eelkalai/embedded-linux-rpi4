// ESP32-C3 as a 3.3V USB-to-serial bridge for a Raspberry Pi 4.
//
//   PC <--native USB (CDC)--> ESP32-C3 <--UART1--> Pi
//
// Wiring (Pi 40-pin header):
//   ESP32 GPIO5 (TX) -> Pi pin 10 (GPIO15, RXD)
//   ESP32 GPIO4 (RX) <- Pi pin 8  (GPIO14, TXD)
//   ESP32 GND        -- Pi pin 6  (GND)
// Do not connect 3V3 or 5V between the boards; each is powered on its own.
//
// GPIO20/21 (the pins labelled RX/TX) are avoided on purpose: the boot ROM
// prints its log on GPIO21 at every reset, which would land in the Pi console.

#include <Arduino.h>

static const uint32_t PI_BAUD = 115200;  // Pi console default
static const int PI_RX_PIN = 4;
static const int PI_TX_PIN = 3;
static const size_t RX_BUFFER_SIZE = 4096;

HardwareSerial PiSerial(1);

static void pump(Stream &from, Stream &to) {
  uint8_t buf[256];
  int n = from.available();
  if (n <= 0) return;
  if (n > (int)sizeof(buf)) n = sizeof(buf);
  n = from.readBytes(buf, n);
  to.write(buf, n);
}

void setup() {
  Serial.setRxBufferSize(RX_BUFFER_SIZE);
  Serial.begin();  // USB CDC, baud is ignored
  PiSerial.setRxBufferSize(RX_BUFFER_SIZE);
  PiSerial.begin(PI_BAUD, SERIAL_8N1, PI_RX_PIN, PI_TX_PIN);
}

void loop() {
  pump(Serial, PiSerial);
  pump(PiSerial, Serial);
}
