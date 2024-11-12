// Etapa 5: Conexión del ESP32 con el Amplificador de celda de carga HX711

#include "HX711.h"

// Definir pines del HX711
const int LOADCELL_DOUT_PIN = 18;
const int LOADCELL_SCK_PIN = 19;

HX711 scale;

void setup() {
  Serial.begin(115200);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
}

void loop() {

  if (scale.is_ready()) {
    long reading = scale.read();
    Serial.print("Leyendo HX711: ");
    Serial.println(reading);
  } else {
    Serial.println("HX711 no encontrado.");
  }

  delay(1000);
  
}
