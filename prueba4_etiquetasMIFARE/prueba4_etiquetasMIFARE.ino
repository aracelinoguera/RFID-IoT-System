// Etapa 4: Configuración de Etiquetas MIFARE Classic 1k

#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <ArduinoJson.h>

// Inicialización del lector RFID PN532
PN532_I2C pn532_i2c(Wire);
PN532 nfc(pn532_i2c);

// Estructura para almacenar los datos del reactivo
struct Reactivo {
  String producto;
  String numero;
  String alta;
  String marca;
  String codigo;
  String presentacion;
  String lote;
  String vencimiento;
  String baja;
};

// Claves predeterminadas para autenticar bloques de la etiqueta RFID
uint8_t keys[][6] = {
  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
};

const int TOTAL_BLOCKS = 63; // Definicion de bloques para almacenar datos

void setup() {
  Serial.begin(115200); // Inicializacion de comunicación serial
  Serial.println("Iniciando sistema RFID para MIFARE Classic 1K...");
  
  nfc.begin(); //Inicialización del lector PN532
  
  // Verificación de conexión y funcionamiento del lector PN532
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("No se encontró el Lector PN532. Verifique las conexiones.");
    while (1); // Si no se encuentra el lector, el programa se detiene
  }
  
  nfc.SAMConfig(); // Configuracion del lector RFID para comunicación segura
  Serial.println("Sistema listo para detectar etiquetas MIFARE Classic 1K.");
}

// Bucle principal
void loop() {
  Serial.println("\nAcerque una etiqueta MIFARE Classic 1K al lector...");
  handleMenu(); // Muestra el menú de opciones
  delay(1000); // Espera 1 segundo antes de repetir el bucle
}

// Función para mostrar y manejar el menú principal
void handleMenu() {
  Serial.println("\nMenú Principal:");
  Serial.println("1. Leer datos del reactivo");
  Serial.println("2. Ingresar nuevos datos del reactivo");
  Serial.println("3. Formatear etiqueta");
  Serial.println("4. Leer todos los sectores");
  Serial.println("5. Mostrar información de la etiqueta");
  Serial.println();
  Serial.print("Seleccione una opción: ");
  Serial.println();

 // Capturar la entrada del usuario
  String option = getUserInput();
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;
  uint8_t sak;

// Leer la etiqueta RFID y su UID (Identificador Único)
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    // Maneja la opción seleccionada por el usuario
    switch (option.toInt()) {
      case 1:
        leerDatosReactivo(uid, uidLength); // Opción para leer datos del reactivo
        break;
      case 2:
        ingresarDatosReactivo(uid, uidLength); // Opción para ingresar nuevos datos
        break;
      case 3:
        formatCard(uid, uidLength); // Opción para formatear la etiqueta
        break;
      case 4:
        printSectorMatrix(uid, uidLength, sak);  // Opción para mostrar sectores de memoria
        break;
      case 5:
        dumpInfo(uid, uidLength); // Opción para mostrar la información básica de la etiqueta
        break;
      default:
        Serial.println("Opción no válida.");
    }
  } else {
    Serial.println("No se detectó ninguna etiqueta. Por favor, acerque una etiqueta al lector.");
  }
}

// Función para capturar la entrada del usuario
String getUserInput() {
  while (!Serial.available()) {
    delay(10); // Espera hasta que el usuario introduzca algo en el monitor serial
  }
  String input = Serial.readStringUntil('\n'); // Captura la entrada del usuario
  input.trim(); // Elimina espacios en blanco al inicio y al final de la entrada
  Serial.println("Entrada recibida: " + input);
  return input; // Devuelve la entrada procesada
}

// Autenticar la etiqueta en un sector específico
bool authenticateCard(uint8_t* uid, uint8_t uidLength) {
  if (!authenticateSector(uid, uidLength, 1)) {
    Serial.print("Fallo en la autenticación del sector ");
    return false;
  }
  Serial.println("Etiqueta autenticada con éxito!");
  return true;
}

// Autentica un sector dado de la etiqueta utilizando la clave predeterminada
bool authenticateSector(uint8_t* uid, uint8_t uidLength, uint8_t sector) {
  uint8_t firstBlockInSector = sector * 4;  
  return nfc.mifareclassic_AuthenticateBlock(uid, uidLength, firstBlockInSector, 0, keys[0]);
}

// Función para leer y mostrar los datos del reactivo almacenados en la etiqueta
void leerDatosReactivo(uint8_t* uid, uint8_t uidLength) {
  byte buffer[16]; // Buffer para almacenar los datos leídos de cada bloque

  // Leer y mostrar los datos del reactivo, evitando bloques reservados (trailer)
  Serial.println();
  Serial.println("Datos del Reactivo:");
  leerBloque(uid, uidLength, 4, buffer, "Producto");  // Bloque 4: Producto
  leerBloque(uid, uidLength, 5, buffer, "Número");  // Bloque 5: Número
  leerBloque(uid, uidLength, 6, buffer, "Fecha de alta");  // Bloque 6: Fecha de alta
  leerBloque(uid, uidLength, 8, buffer, "Marca");  // Bloque 8: Marca
  leerBloque(uid, uidLength, 9, buffer, "Código");  // Bloque 9: Código
  leerBloque(uid, uidLength, 10, buffer, "Presentación");  // Bloque 10: Presentación
  leerBloque(uid, uidLength, 12, buffer, "Lote");  // Bloque 12: Lote
  leerBloque(uid, uidLength, 13, buffer, "Fecha de vencimiento");  // Bloque 13: Fecha de vencimiento
  leerBloque(uid, uidLength, 14, buffer, "Fecha de baja (opcional)");  // Bloque 14: Fecha de baja
}

// Función para autenticar, leer e imprimir el contenido de un bloque de la etiqueta
void leerBloque(uint8_t* uid, uint8_t uidLength, int block, byte* buffer, const char* titulo) {
  // Autenticación con clave A para el bloque dado
  if (!nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, 0, keys[0])) {
    Serial.print(F("Error de autenticación en el bloque ")); Serial.println(block);
    return;
  }

  // Leer los datos del bloque de memoria
  if (nfc.mifareclassic_ReadDataBlock(block, buffer)) {
    Serial.print(titulo); 
    Serial.print(": ");  // Imprime el título del dato ingresado
    bool hasData = false;
    for (int i = 0; i < 16; i++) {
      if (isPrintable(buffer[i])) {  // Imprime cada byte como carácter solo si es imprimible
        Serial.print((char)buffer[i]);  // Imprime directamente los caracteres, incluidos los espacios
        hasData = true;
      }
    }
    // Si no hay datos, informa que no se ingresaron datos
    if (!hasData) {
      Serial.println("No se ingresaron datos");
    } else {
      Serial.println();  // Salto de línea al final del dato solo si hubo datos
    }
  } else {
    Serial.print(F("Error al leer el bloque ")); Serial.println(block);
  }
}

// Función para ingresar los datos del reactivo
void ingresarDatosReactivo(uint8_t* uid, uint8_t uidLength) {
  byte buffer[16]; // Buffer para almacenar los datos a escribir
  byte block;
  int len;

  // Solicitar y escribir los datos del reactivo en los bloques correspondientes
  Serial.setTimeout(20000L);  // Espera hasta 20 segundos por la entrada serial
  
  // Ingreso de datos del nombre del reactivo y almacenamiento en bloque 4
  Serial.println(F("Escribe el nombre del producto seguido de #"));
  len = Serial.readBytesUntil('#', (char *) buffer, 16); // Leer nombre del producto con espacios
  for (byte i = len; i < 16; i++) buffer[i] = ' ';       // Rellenar con espacios si es necesario
  block = 4;  // Bloque donde se escribirá el nombre del producto
  escribirBloque(uid, uidLength, block, buffer, "Producto");

  // Ingreso de datos del número del reactivo y almacenamiento en bloque 5
  Serial.println(F("Escribe el número del reactivo seguido de #"));
  len = Serial.readBytesUntil('#', (char *) buffer, 16); // Leer número del reactivo con espacios
  for (byte i = len; i < 16; i++) buffer[i] = ' ';       // Rellenar con espacios si es necesario
  block = 5;  // Bloque donde se escribirá el número del reactivo
  escribirBloque(uid, uidLength, block, buffer, "Número");

  // Ingreso de datos de fecha de alta del reactivo y almacenamiento en bloque 6
  Serial.println(F("Escribe la fecha de alta (DD/MM/AAAA) seguida de #"));
  len = Serial.readBytesUntil('#', (char *) buffer, 16); // Leer fecha de alta
  for (byte i = len; i < 16; i++) buffer[i] = ' ';       // Rellenar con espacios si es necesario
  block = 6;  // Bloque donde se escribirá la fecha de alta
  escribirBloque(uid, uidLength, block, buffer, "Fecha de alta");

  // Ingreso de datos de marca del reactivo y almacenamiento en bloque 8
  Serial.println(F("Escribe la marca seguida de #"));
  len = Serial.readBytesUntil('#', (char *) buffer, 16); // Leer marca con espacios
  for (byte i = len; i < 16; i++) buffer[i] = ' ';       // Rellenar con espacios si es necesario
  block = 8;  // Bloque donde se escribirá la marca
  escribirBloque(uid, uidLength, block, buffer, "Marca");

  // Ingreso de datos del código del reactivo y almacenamiento en bloque 9
  Serial.println(F("Escribe el código seguido de #"));
  len = Serial.readBytesUntil('#', (char *) buffer, 16); // Leer código con espacios
  for (byte i = len; i < 16; i++) buffer[i] = ' ';       // Rellenar con espacios si es necesario
  block = 9;  // Bloque donde se escribirá el código
  escribirBloque(uid, uidLength, block, buffer, "Código");

  // Ingreso de datos de presentación del reactivo y almacenamiento en bloque 10
  Serial.println(F("Escribe la presentación seguida de #"));
  len = Serial.readBytesUntil('#', (char *) buffer, 16); // Leer presentación con espacios
  for (byte i = len; i < 16; i++) buffer[i] = ' ';       // Rellenar con espacios si es necesario
  block = 10;  // Bloque donde se escribirá la presentación
  escribirBloque(uid, uidLength, block, buffer, "Presentación");

  // Ingreso de datos del lote del reactivo y almacenamiento en bloque 12
  Serial.println(F("Escribe el lote seguido de #"));
  len = Serial.readBytesUntil('#', (char *) buffer, 16); // Leer lote con espacios
  for (byte i = len; i < 16; i++) buffer[i] = ' ';       // Rellenar con espacios si es necesario
  block = 12;  // Bloque donde se escribirá el lote
  escribirBloque(uid, uidLength, block, buffer, "Lote");

  // Ingreso de datos de fecha de vencimiento del reactivo y almacenamiento en bloque 13
  Serial.println(F("Escribe la fecha de vencimiento (DD/MM/AAAA) seguida de #"));
  len = Serial.readBytesUntil('#', (char *) buffer, 16); // Leer fecha de vencimiento
  for (byte i = len; i < 16; i++) buffer[i] = ' ';       // Rellenar con espacios si es necesario
  block = 13;  // Bloque donde se escribirá la fecha de vencimiento
  escribirBloque(uid, uidLength, block, buffer, "Fecha de vencimiento");

  // Ingreso de datos de fecha de baja del reactivo (opcional)
  Serial.println(F("Escribe la fecha de baja (opcional) seguida de #"));
  len = Serial.readBytesUntil('#', (char *) buffer, 16); // Leer fecha de baja (opcional)
  for (byte i = len; i < 16; i++) buffer[i] = ' ';       // Rellenar con espacios si es necesario
  block = 14;  // Bloque donde se escribirá la baja
  escribirBloque(uid, uidLength, block, buffer, "Fecha de baja (opcional)");
}

// Función para escribir datos en un bloque específico
void escribirBloque(uint8_t* uid, uint8_t uidLength, int block, byte* buffer, const char* titulo) {
  // Autenticar con clave A para el bloque dado
  if (!nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, 0, keys[0])) {
    Serial.print(F("Autenticación fallida en el bloque ")); Serial.println(block);
    return;
  }

  // Escribir los datos en el bloque si la autenticación es exitosa 
  if (!nfc.mifareclassic_WriteDataBlock(block, buffer)) {
    Serial.print(F("Error al escribir el bloque ")); Serial.println(block);
    return;
  } else {
    Serial.print(titulo); Serial.println(F(" guardado correctamente"));
  }
}

// Función para mostrar información general de la etiqueta
void dumpInfo(uint8_t* uid, uint8_t uidLength) {
  Serial.println("Información de la etiqueta MIFARE Classic:");
  Serial.print("  UID: ");
  for (uint8_t i = 0; i < uidLength; i++) {
    Serial.print(" 0x");
    Serial.print(uid[i], HEX);
  }
  Serial.println("");

  Serial.println("  Tipo: MIFARE Classic 1K");
  Serial.println("  Capacidad: 1024 bytes, organizados en 16 sectores de 4 bloques cada uno");
  Serial.println("  Cada bloque contiene 16 bytes");
}

// Función para mostrar la matriz de sectores y bloques de la etiqueta
void printSectorMatrix(uint8_t* uid, uint8_t uidLength, uint8_t sak) {
  Serial.println("\nMatriz de datos de la etiqueta MIFARE Classic 1K:");
  
  // Imprimir UID y SAK
  Serial.print("Card UID: ");
  for (int i = 0; i < uidLength; i++) {
    Serial.print(uid[i], HEX);
    if (i < uidLength - 1) {
      Serial.print(":");
    }
  }
  Serial.println();
  
  Serial.print("Card SAK: ");
  Serial.println(sak, HEX);
  
  Serial.println("PICC type: MIFARE 1KB\n");

  // Recorrer todos los sectores y bloques mostrando sus datos
  for (int sector = 0; sector < 16; sector++) {
    Serial.print("Sector "); Serial.println(sector);
    for (int block = 0; block < 4; block++) {
      uint8_t blockAddress = sector * 4 + block;
      uint8_t data[16];
      
      // Autenticar y leer cada bloque
      if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, blockAddress, 0, keys[0])) {
        if (nfc.mifareclassic_ReadDataBlock(blockAddress, data)) {
          Serial.print("  Bloque "); Serial.print(blockAddress); Serial.print(": ");
          for (int i = 0; i < 16; i++) {
            if (data[i] < 0x10) {
              Serial.print("0");
            }
            Serial.print(data[i], HEX);
            Serial.print(" ");
          }
          Serial.println();
        } else {
          Serial.print("  Error al leer el bloque "); Serial.println(blockAddress);
        }
      } else {
        Serial.print("  Error de autenticación en el sector "); Serial.println(sector);
        break;  // Salir del bucle del sector si no se puede autenticar
      }
    }
  }
}

// Función para formatear una etiqueta RFID, limpiando los bloques
void formatCard(uint8_t* uid, uint8_t uidLength) {
  Serial.println("Formateando etiqueta...");
  for (uint8_t sector = 1; sector < 16; sector++) {
    if (authenticateSector(uid, uidLength, sector)) {
      for (uint8_t block = 0; block < 3; block++) {
        uint8_t blockAddress = sector * 4 + block;
        uint8_t emptyBlock[16] = {0};
        if (nfc.mifareclassic_WriteDataBlock(blockAddress, emptyBlock)) {
          Serial.print("Bloque "); Serial.print(blockAddress); Serial.println(" formateado.");
        } else {
          Serial.print("Error al formatear el bloque "); Serial.println(blockAddress);
        }
      }
    } else {
      Serial.print("No se pudo autenticar el sector "); Serial.println(sector);
    }
  }
  Serial.println("Formateo completado.");
}
