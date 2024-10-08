// Etapa 3: Conexión del Arduino al PN532

#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

// Inicializar la comunicación I2C con el lector PN532
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);

bool cardDetected = false; // Variable para rastrear el estado de detección de tarjeta

void setup(void) {
  Serial.begin(115200);
  Serial.println("Inicializando el lector RFID PN532...");

  // Inicializar el lector NFC
  nfc.begin();
  Serial.println("Esperando una etiqueta MIFARE...");  // Imprimir una vez al inicio
}

void loop(void) {
  if (nfc.tagPresent()) {  // Verificar si hay una tarjeta presente
    if (!cardDetected) {   // Solo imprimir cuando una tarjeta se detecta por primera vez
      NfcTag tag = nfc.read();  // Leer la tarjeta
      Serial.println("Tarjeta MIFARE detectada:");
      tag.print();  // Imprimir la información de la tarjeta en el Monitor Serial
      cardDetected = true;  // Marcar la tarjeta como detectada
    }
  } else {
    // Si no se detecta ninguna tarjeta, volver a esperar una nueva
    if (cardDetected) {
      Serial.println("Esperando una etiqueta MIFARE...");
      cardDetected = false;  // Reiniciar la detección para buscar una nueva tarjeta
    }
  }
  delay(500);  // Pausa corta para evitar chequeos demasiado rápidos
}