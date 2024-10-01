#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <FirebaseESP32.h>
#include <WiFi.h>

// Inicialización del lector RFID PN532
PN532_I2C pn532_i2c(Wire);
PN532 nfc(pn532_i2c);

// Configuración de Firebase
FirebaseConfig config;
FirebaseAuth auth;
FirebaseData firebaseData;

// Configuración WiFi
const char* ssid = "Flia N.E";
const char* password = "tania946";

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
  Serial.begin(115200);
  Serial.println("Iniciando sistema RFID para MIFARE Classic 1K...");

  // Conectar a la red WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi conectado.");

  // Configuración de Firebase
  config.host = "reactivos-59061-default-rtdb.firebaseio.com"; 
  config.api_key = "AIzaSyDFRgpkGVGw-Da_XZq86rTDvN-pv0OdNhQ"; 
 
  // Autenticación anónima
  config.signer.tokens.legacy_token = "";  // Dejar vacío para autenticación anónima

  // Inicializar Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Conexión a Firebase exitosa.");

  // Inicialización del lector PN532
  nfc.begin();

  // Verificación de conexión y funcionamiento del lector PN532
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("No se encontró el Lector PN532. Verifique las conexiones.");
    while (1); // Si no se encuentra el lector, el programa se detiene
  }

  nfc.SAMConfig(); // Configuración del lector RFID para comunicación segura
  Serial.println("Sistema listo para detectar etiquetas MIFARE Classic 1K.");
}

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
  String option = getUserInput();
  
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;
  
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
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
        printSectorMatrix(uid, uidLength, 0);  // Opción para mostrar sectores de memoria
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

// Función para leer y mostrar los datos del reactivo almacenados en la etiqueta
void leerDatosReactivo(uint8_t* uid, uint8_t uidLength) {
  byte buffer[16]; // Buffer para almacenar los datos leídos de cada bloque

  Reactivo reactivo;

   // Declarar variables String para pasar a la función leerBloque
  String producto = "";
  String numero = "";
  String alta = "";
  String marca = "";
  String codigo = "";
  String presentacion = "";
  String lote = "";
  String vencimiento = "";
  String baja = "";

  // Leer y mostrar los datos del reactivo, evitando bloques reservados (trailer)
  Serial.println();
  Serial.println("Datos del Reactivo:");
   leerBloque(uid, uidLength, 4, buffer, producto);
  leerBloque(uid, uidLength, 5, buffer, numero);
  leerBloque(uid, uidLength, 6, buffer, alta);
  leerBloque(uid, uidLength, 8, buffer, marca);
  leerBloque(uid, uidLength, 9, buffer, codigo);
  leerBloque(uid, uidLength, 10, buffer, presentacion);
  leerBloque(uid, uidLength, 12, buffer, lote);
  leerBloque(uid, uidLength, 13, buffer, vencimiento);
  leerBloque(uid, uidLength, 14, buffer, baja);

  reactivo.producto = producto;
  reactivo.numero = numero;
  reactivo.alta = alta;
  reactivo.marca = marca;
  reactivo.codigo = codigo;
  reactivo.presentacion = presentacion;
  reactivo.lote = lote;
  reactivo.vencimiento = vencimiento;
  reactivo.baja = baja;

  // Enviar los datos a Firebase
  enviarDatosAFirebase(reactivo);
}

// Función para ingresar los datos del reactivo
void ingresarDatosReactivo(uint8_t* uid, uint8_t uidLength) {
  byte buffer[16]; // Buffer para almacenar los datos a escribir
  Reactivo reactivo;
  byte block;
  int len;

  // Solicitar y escribir los datos del reactivo en los bloques correspondientes
  Serial.setTimeout(20000L);  // Espera hasta 20 segundos por la entrada serial

  // Ingreso de datos del nombre del reactivo y almacenamiento en bloque 4
  Serial.println(F("Escribe el nombre del producto seguido de #"));
  len = Serial.readBytesUntil('#', (char *) buffer, 16);
  for (byte i = len; i < 16; i++) buffer[i] = ' ';
  block = 4;
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
    Serial.print(F("Autenticación fallida en el bloque ")); 
    Serial.println(block);
    return;
  }

  // Escribir los datos en el bloque si la autenticación es exitosa 
  if (!nfc.mifareclassic_WriteDataBlock(block, buffer)) {
    Serial.print(F("Error al escribir el bloque ")); 
    Serial.println(block);
    return;
  } else {
    Serial.print(titulo); 
    Serial.println(F(" guardado correctamente"));
  }
}

// Función para leer un bloque y almacenarlo en una variable
void leerBloque(uint8_t* uid, uint8_t uidLength, int block, byte* buffer, String &destino) {
  if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, 0, keys[0])) {
    if (nfc.mifareclassic_ReadDataBlock(block, buffer)) {
      destino = "";
      for (int i = 0; i < 16; i++) {
        if (isPrintable(buffer[i])) {
          destino += (char)buffer[i];
        }
      }
    }
  }
}


// Función para enviar los datos a Firebase
void enviarDatosAFirebase(Reactivo reactivo) {
  String path = "/reactivos/" + reactivo.producto;

  if (Firebase.setString(firebaseData, path + "/producto", reactivo.producto) &&
      Firebase.setString(firebaseData, path + "/numero", reactivo.numero) &&
      Firebase.setString(firebaseData, path + "/alta", reactivo.alta) &&
      Firebase.setString(firebaseData, path + "/marca", reactivo.marca) &&
      Firebase.setString(firebaseData, path + "/codigo", reactivo.codigo) &&
      Firebase.setString(firebaseData, path + "/presentacion", reactivo.presentacion) &&
      Firebase.setString(firebaseData, path + "/lote", reactivo.lote) &&
      Firebase.setString(firebaseData, path + "/vencimiento", reactivo.vencimiento) &&
      Firebase.setString(firebaseData, path + "/baja", reactivo.baja)) {
    Serial.println("Datos enviados a Firebase correctamente.");
  } else {
    Serial.println("Error al enviar los datos a Firebase.");
    Serial.println(firebaseData.errorReason());
  }
}



// Función para formatear una etiqueta RFID
void formatCard(uint8_t* uid, uint8_t uidLength) {
  Serial.println("Formateando etiqueta...");
  for (uint8_t sector = 1; sector < 16; sector++) {
    if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, sector * 4, 0, keys[0])) {
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

// Función para mostrar la matriz de sectores y bloques de la etiqueta
void printSectorMatrix(uint8_t* uid, uint8_t uidLength, uint8_t sak) {
  Serial.println("\nMatriz de datos de la etiqueta MIFARE Classic 1K:");
  for (int sector = 0; sector < 16; sector++) {
    Serial.print("Sector "); Serial.println(sector);
    for (int block = 0; block < 4; block++) {
      uint8_t blockAddress = sector * 4 + block;
      uint8_t data[16];
      if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, blockAddress, 0, keys[0])) {
        if (nfc.mifareclassic_ReadDataBlock(blockAddress, data)) {
          Serial.print("  Bloque "); Serial.print(blockAddress); Serial.print(": ");
          for (int i = 0; i < 16; i++) {
            if (data[i] < 0x10) Serial.print("0");
            Serial.print(data[i], HEX);
            Serial.print(" ");
          }
          Serial.println();
        } else {
          Serial.print("  Error al leer el bloque "); Serial.println(blockAddress);
        }
      } else {
        Serial.print("  Error de autenticación en el sector "); Serial.println(sector);
        break;
      }
    }
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