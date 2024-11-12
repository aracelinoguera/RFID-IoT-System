// Etapa 1: Verificación de correcta importación de Librerías

#include <WiFi.h>             // Librería para conexión WiFi
#include <FirebaseESP32.h>    // Librería para manejar la conexión con Firebase
#include <Wire.h>             // Librería para manejar el protocolo I2C
#include <Adafruit_PN532.h>   // Librería para manejar el lector RFID PN532
#include "HX711.h"            // Librería para manejar el amplificador de celdas de carga
#include "DHT.h"              // Librería para manejar el sensor de temperatura y humedad

void setup() {
  // Inicializar la comunicación serial a 115200 baudios
  Serial.begin(115200);

  // Imprimir el mensaje de verificación
  Serial.println("Las librerías han sido correctamente incluidas.");
}

void loop() {
  // No es necesario ejecutar nada en el bucle porque solo se verifica la inclusión de librerías.
}
