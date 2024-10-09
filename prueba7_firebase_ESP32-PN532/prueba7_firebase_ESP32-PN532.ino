#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <FirebaseESP32.h>
#include <WiFi.h>

// Configuración de credenciales de WiFi y Firebase
#define WIFI_SSID "Nombre_de_Red"           // Nombre de la red WiFi
#define WIFI_PASSWORD "Contraseña_de_Red"       // Contraseña de la red WiFi
#define FIREBASE_HOST "URL (.firebaseio.com)"  // URL de la base de datos de Firebase
#define FIREBASE_AUTH "Token_de_Autenticación"    // Token de autenticación de Firebase

// Inicialización de objetos PN532 y Firebase
PN532_I2C pn532_i2c(Wire);
PN532 nfc(pn532_i2c);
FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

// Estructura para almacenar los datos del reactivo
struct Reactivo {
  String producto, numero, alta, marca, codigo, presentacion, lote, vencimiento, baja;
};

// Clave predeterminada para autenticar los bloques en las etiquetas RFID
const uint8_t DEFAULT_KEY[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const int TOTAL_BLOCKS = 63;  // Número total de bloques en la tarjeta MIFARE Classic 1K

// Función setup: inicializa el sistema y los periféricos
void setup() {
  Serial.begin(115200);  // Inicialización del puerto serie a 115200 baudios
  while (!Serial) delay(10);  // Espera a que el puerto serie esté listo

  Serial.println();
  Serial.println("Iniciando...");
  
  // Inicialización de WiFi, Firebase y el lector NFC
  initWiFi();
  initFirebase();
  initNFC();

  Serial.printf("Setup completado. Memoria libre: %d bytes\n", ESP.getFreeHeap());
}

// Función loop: se ejecuta repetidamente mostrando el menú
void loop() {
  Serial.println("\nAcerque una etiqueta MIFARE Classic 1K al lector...");
  Serial.println("\nMenú Principal:");
  Serial.println("1. Leer datos");
  Serial.println("2. Ingresar datos");
  Serial.println("3. Editar datos");
  Serial.println("4. Formatear etiqueta");
  Serial.println("5. Leer todos los sectores");
  Serial.println("6. Mostrar información");
  Serial.println("7. Salir");
  Serial.print("\nSeleccione una opción: ");

  while (!Serial.available()) {
    delay(100);  // Espera a que el usuario ingrese una opción
  }

  int option = Serial.parseInt();
  Serial.println(option);  // Eco de la opción seleccionada

  // Limpia el buffer del Serial
  while (Serial.available()) {
    Serial.read();
  }

  uint8_t uid[7];
  uint8_t uidLength;

  if (option != 7) {  // Si la opción no es salir
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
      Serial.println();
      Serial.println("Etiqueta detectada.");
      switch (option) {
        case 1: leerDatosReactivo(uid, uidLength); break;
        case 2: ingresarDatosReactivo(uid, uidLength); break;
        case 3: editarDatosReactivo(uid, uidLength); break;
        case 4: formatCard(uid, uidLength); break;
        case 5: printSectorMatrix(uid, uidLength); break;
        case 6: dumpInfo(uid, uidLength); break;
        default: Serial.println("Opción no válida.");
      }
    } else {
      Serial.println("No se detectó ninguna etiqueta.");
    }
  } else {
    Serial.println("Saliendo del programa...");
    // Aquí puedes agregar código para una salida segura si es necesario
  }

  Serial.println("Presione cualquier tecla para continuar...\n");
  while (!Serial.available()) {
    delay(100);
  }
  while (Serial.available()) {
    Serial.read();  // Limpia el buffer
  }

  delay(1000);  // Pequeña pausa antes de mostrar el menú de nuevo
}


// Inicializa la conexión WiFi
void initWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // Conectar a la red WiFi
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");  // Mostrar puntos hasta que se conecte
    delay(300);
  }
  Serial.println("\nConectado con IP: " + WiFi.localIP().toString());  // Imprime la dirección IP obtenida
}

// Inicializa la conexión a Firebase
void initFirebase() {
  config.database_url = FIREBASE_HOST;  // URL de la base de datos de Firebase
  config.signer.tokens.legacy_token = FIREBASE_AUTH;  // Token de autenticación de Firebase
  Firebase.begin(&config, &auth);  // Inicia la conexión a Firebase
  Firebase.reconnectWiFi(true);  // Permitir la reconexión automática a WiFi si se pierde la conexión
  Serial.println("Conexión a Firebase exitosa.");
}

// Inicializa el lector PN532
void initNFC() {
  nfc.begin();  // Inicia la comunicación con el lector
  uint32_t versiondata = nfc.getFirmwareVersion();  // Obtiene la versión del firmware del lector
  if (!versiondata) {
    Serial.println("No se encontró el Lector PN532. Verifique las conexiones.");
    while (1);  // Detiene el programa si no se detecta el lector
  }
  nfc.SAMConfig();  // Configuración del lector
  Serial.println();
  Serial.println("Sistema listo para detectar etiquetas MIFARE Classic 1K.");
}

// Función para leer datos almacenados en la etiqueta 
void leerDatosReactivo(uint8_t* uid, uint8_t uidLength) {
  if (!uid || uidLength == 0) {
    Serial.println("UID inválido");
    return;
  }

  // Convertir el UID a un string
  String uidString = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    // Hacer que el string sea de 2 dígitos, agregando ceros si es necesario
    if (uid[i] < 0x10) uidString += "0";
    uidString += String(uid[i], HEX);
  }

  Serial.println("UID generado: " + uidString);


  Reactivo reactivo;
  int bloques[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};  // Bloques de memoria donde se almacenan los datos
  String* campos[] = {&reactivo.producto, &reactivo.numero, &reactivo.alta, &reactivo.marca, 
                      &reactivo.codigo, &reactivo.presentacion, &reactivo.lote, &reactivo.vencimiento, &reactivo.baja};

  Serial.println("\nDatos del Reactivo:");
  for (int i = 0; i < 9; i++) {
    if (leerBloque(uid, uidLength, bloques[i], *campos[i])) {
      Serial.println(campos[i]->c_str());  // Imprime el campo leído
    } else {
      Serial.println("Error leyendo bloque " + String(bloques[i]));  // Error al leer un bloque
    }
  }

   // Crear la ruta usando el UID como clave
  String path = uidString;

  Serial.println();
  Serial.println("Creando la ruta en Firebase: " + path);  // Imprime la ruta para verificarla

// Enviar datos a Firebase
  Serial.println();
  Serial.println("Enviando datos...");
  enviarDatosAFirebase(reactivo, path);
}

  
// Función para leer los datos almacenados en la etiqueta sin enviar datos a Firebase
void leerDatosReactivoEdit(uint8_t* uid, uint8_t uidLength) {
  if (!uid || uidLength == 0) {
    Serial.println("UID inválido");
    return;
  }

  Reactivo reactivo;
  int bloques[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};  // Bloques de memoria donde se almacenan los datos
  String* campos[] = {&reactivo.producto, &reactivo.numero, &reactivo.alta, &reactivo.marca, 
                      &reactivo.codigo, &reactivo.presentacion, &reactivo.lote, &reactivo.vencimiento, &reactivo.baja};

  Serial.println("\nDatos del Reactivo:");
  for (int i = 0; i < 9; i++) {
    if (leerBloque(uid, uidLength, bloques[i], *campos[i])) {
      Serial.println(campos[i]->c_str());  // Imprime el campo leído
    } else {
      Serial.println("Error leyendo bloque " + String(bloques[i]));  // Error al leer un bloque
    }
  }
}

// Función para leer datos almacenados en la etiqueta y comprobar si el UID ya existe en la base de datos
void leerDatosReactivoUID(uint8_t* uid, uint8_t uidLength) {
  if (!uid || uidLength == 0) {
    Serial.println("UID inválido");
    return;
  }

  // Convertir el UID a un string
  String uidString = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    // Hacer que el string sea de 2 dígitos, agregando ceros si es necesario
    if (uid[i] < 0x10) uidString += "0";
    uidString += String(uid[i], HEX);
  }

  Serial.println("UID generado: " + uidString);


  Reactivo reactivo;
  int bloques[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};  // Bloques de memoria donde se almacenan los datos
  String* campos[] = {&reactivo.producto, &reactivo.numero, &reactivo.alta, &reactivo.marca, 
                      &reactivo.codigo, &reactivo.presentacion, &reactivo.lote, &reactivo.vencimiento, &reactivo.baja};

  Serial.println("\nDatos del Reactivo:");
  for (int i = 0; i < 9; i++) {
    if (leerBloque(uid, uidLength, bloques[i], *campos[i])) {
      Serial.println(campos[i]->c_str());  // Imprime el campo leído
    } else {
      Serial.println("Error leyendo bloque " + String(bloques[i]));  // Error al leer un bloque
    }
  }

  String path = uidString;

  Serial.println("\nVerificando UID " + path + " en la ruta de Firebase...");  // Imprime la ruta para verificarla
  Serial.println();

  // Verificar si el UID ya existe en Firebase
  if (Firebase.get(firebaseData, "/reactivos/" + path)) {
    if (firebaseData.dataType() == "null" || !firebaseData.dataAvailable()) {
      // Si no existe, crear un nuevo registro en Firebase
      Serial.println("Etiqueta no encontrada. Registra la etiqueta antes de editarla\n");
    } else {
      // Si ya existe, actualizar los datos en Firebase
      Serial.println("Etiqueta encontrada, enviando información a Firebase...");
      Serial.println();
      enviarDatosAFirebase(reactivo, path);  // Actualiza los datos en Firebase
      Serial.println("La información ha sido correctamente actualizada!\n.");
    }
  } else {
    Serial.println("Error al verificar la etiqueta en Firebase: " + firebaseData.errorReason());
  }
}

// Función para ingresar nuevos datos a la etiqueta NFC
void ingresarDatosReactivo(uint8_t* uid, uint8_t uidLength) {
  String campos[] = {"producto", "numero", "alta", "marca", "codigo", "presentacion", "lote", "vencimiento", "baja"};  // Campos de datos
  int bloques[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};  // Bloques donde se almacenan los datos

  for (int i = 0; i < 9; i++) {
    Serial.print("Ingrese ");
    Serial.print(campos[i]);
    Serial.println(" (máx. 16 caracteres):");
    
    while (!Serial.available()) delay(10);
    String dato = Serial.readStringUntil('\n');  // Lee la entrada del usuario
    dato.trim();
    
    uint8_t buffer[16] = {0};  // Almacena los datos ingresados
    dato.getBytes(buffer, 16);
    escribirBloque(uid, uidLength, bloques[i], buffer, campos[i].c_str());  // Escribe los datos en el bloque correspondiente
  }
}

// Función para leer un bloque de datos de la etiqueta
bool leerBloque(uint8_t* uid, uint8_t uidLength, int block, String &destino) {
  uint8_t buffer[16];  // Buffer para almacenar los datos leídos
  if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, 0, const_cast<uint8_t*>(DEFAULT_KEY))) {
    if (nfc.mifareclassic_ReadDataBlock(block, buffer)) {
      destino = String((char*)buffer).substring(0, 16);  // Convierte los datos en una cadena de caracteres
      destino.trim();  // Elimina espacios en blanco al inicio y final
      return true;
    }
  }
  Serial.print("Error al leer el bloque ");
  Serial.println(block);
  return false;
}

// Función para escribir un bloque en la etiqueta NFC
void escribirBloque(uint8_t* uid, uint8_t uidLength, int block, uint8_t* buffer, const char* titulo) {
  if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, 0, const_cast<uint8_t*>(DEFAULT_KEY))) {
    if (nfc.mifareclassic_WriteDataBlock(block, buffer)) {
      Serial.print(titulo);
      Serial.println(" editado exitosamente");
      Serial.println();
    } else {
      Serial.print("Error al escribir el bloque ");
      Serial.println(block);
    }
  } else {
    Serial.print("Error de autenticación en el bloque ");
    Serial.println(block);
  }
}

// Función para enviar los datos del reactivo a Firebase usando el UID como clave primaria
void enviarDatosAFirebase(const Reactivo& reactivo, const String& uidString) {
  
  // Usar el UID como clave principal en la ruta
  String path = uidString;  // Ruta donde se almacenarán los datos en Firebase

  if (path.length() == 0) {
    Serial.println("Path inválido para Firebase");
    return;
  }

  // Definir los campos y valores asociados al reactivo
  String campos[] = {"01_producto", "02_numero", "03_alta", "04_marca", "05_codigo", "06_presentacion", "07_lote", "08_vencimiento", "09_baja"};  // Campos de datos
  const String* valores[] = {&reactivo.producto, &reactivo.numero, &reactivo.alta, &reactivo.marca, 
                             &reactivo.codigo, &reactivo.presentacion, &reactivo.lote, &reactivo.vencimiento, &reactivo.baja};

  bool success = true;  // Verificar si todos los envíos fueron exitosos
  for (int i = 0; i < 9; i++) {
    if (valores[i] && valores[i]->length() > 0) {
      // Carpeta para almacenar información de reactivos
      if (!Firebase.setString(firebaseData, "/reactivos/" + path + "/" + campos[i], *valores[i])) {
        success = false;
        Serial.println("Error al enviar " + campos[i] + ": " + firebaseData.errorReason());  // Error al enviar un campo a Firebase
      }
    } else {
      Serial.println("Valor vacío para " + campos[i] + ", no se enviará a Firebase");  // No se envía si el valor está vacío
    }
  }

  if (success) {
    Serial.println("Datos enviados con éxito.\n");  // Todos los datos se enviaron con éxito
  } else {
    Serial.println("Hubo errores al enviar los datos.");  // Algunos datos fallaron al enviarse
  }
}

// Función para editar etiqueta
void editarDatosReactivo(uint8_t* uid, uint8_t uidLength) {
  Reactivo reactivo;
  
  leerDatosReactivoEdit(uid, uidLength);

  // Convertir el UID a un string
  String uidString = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    // Hacer que el string sea de 2 dígitos, agregando ceros si es necesario
    if (uid[i] < 0x10) uidString += "0";
    uidString += String(uid[i], HEX);
  }

  String path = uidString;

  Serial.println("\nVerificando la ruta en Firebase: " + path);  // Imprime la ruta para verificarla
  Serial.println();

  // Verificar si el UID ya existe en Firebase
  if (Firebase.get(firebaseData, "/reactivos/" + path)) {
    if (firebaseData.dataType() == "null" || !firebaseData.dataAvailable()) {
      // Si no existe, crear un nuevo registro en Firebase
      Serial.println("Etiqueta no encontrada. Registra la etiqueta antes de editarla\n");
    } else {
      // Si ya existe, actualizar los datos en Firebase
      Serial.println("UID encontrado!\n");
    }
  } else { // Este else cierra el if principal
    Serial.println("Error al verificar la etiqueta en Firebase: " + firebaseData.errorReason());
  }

  String campos[] = {"producto", "numero", "alta", "marca", "codigo", "presentacion", "lote", "vencimiento", "baja"};
  int bloques[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};
  
  Serial.println("\n¿Qué campo(s) desea editar? (Ingrese los números separados por comas, o 0 para editar todos)");
  for (int i = 0; i < 9; i++) {
    Serial.printf("%d. %s\n", i + 1, campos[i].c_str());
  }

  while (!Serial.available()) {
    delay(100);
  }
  String input = Serial.readStringUntil('\n');
  input.trim();

  bool editarTodos = (input == "0");
  
  for (int i = 0; i < 9; i++) {
    if (editarTodos || input.indexOf(String(i + 1)) != -1) {
      Serial.println();
      Serial.printf("Ingrese nuevo valor para %s (máx. 16 caracteres):\n", campos[i].c_str());
      while (!Serial.available()) {
        delay(100);
      }
      String nuevoValor = Serial.readStringUntil('\n');
      nuevoValor.trim();

      uint8_t buffer[16] = {0};
      nuevoValor.getBytes(buffer, 16);
      Serial.println("Procesando cambios...");
      escribirBloque(uid, uidLength, bloques[i], buffer, campos[i].c_str());
    }
  }

  Serial.println("Leyendo nuevos datos...");
  Serial.println();
  leerDatosReactivoUID(uid, uidLength); 
  Serial.println();
}

// Función para formatear una tarjeta NFC
void formatCard(uint8_t* uid, uint8_t uidLength) {
  Serial.println("Formateando etiqueta...");
  uint8_t emptyBlock[16] = {0};  // Bloque vacío para sobrescribir la tarjeta

  // Recorre todos los sectores y bloques de la tarjeta
  for (uint8_t sector = 1; sector < 16; sector++) {
    if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, sector * 4, 0, const_cast<uint8_t*>(DEFAULT_KEY))) {
      for (uint8_t block = 0; block < 3; block++) {
        uint8_t blockAddress = sector * 4 + block;
        if (nfc.mifareclassic_WriteDataBlock(blockAddress, emptyBlock)) {
          Serial.print("Bloque ");
          Serial.print(blockAddress);
          Serial.println(" formateado.");
        } else {
          Serial.print("Error al formatear el bloque ");
          Serial.println(blockAddress);
        }
      }
    } else {
      Serial.print("No se pudo autenticar el sector ");
      Serial.println(sector);
    }
  }
  Serial.println("Formateo completado.");
}

// Función para imprimir una matriz de los datos de la etiqueta NFC
void printSectorMatrix(uint8_t* uid, uint8_t uidLength) {
  Serial.println("\nMatriz de datos de la etiqueta MIFARE Classic 1K:");
  for (int sector = 0; sector < 16; sector++) {
    Serial.print("Sector ");
    Serial.println(sector);
    for (int block = 0; block < 4; block++) {
      uint8_t blockAddress = sector * 4 + block;
      uint8_t data[16];  // Buffer para leer los datos del bloque
      if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, blockAddress, 0, const_cast<uint8_t*>(DEFAULT_KEY)) &&
          nfc.mifareclassic_ReadDataBlock(blockAddress, data)) {
        Serial.print("  Bloque ");
        Serial.print(blockAddress);
        Serial.print(": ");
        for (int i = 0; i < 16; i++) {
          if (data[i] < 0x10) Serial.print("0");
          Serial.print(data[i], HEX);  // Imprime los datos en formato hexadecimal
          Serial.print(" ");
        }
        Serial.println();
      } else {
        Serial.print("  Error en el bloque ");
        Serial.println(blockAddress);
        break;
      }
    }
  }
}

// Función para mostrar información básica de la etiqueta NFC
void dumpInfo(uint8_t* uid, uint8_t uidLength) {
  Serial.println("Información de la etiqueta MIFARE Classic:");
  Serial.print("  UID: ");
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);  // Imprime el UID en formato hexadecimal
    Serial.print(" ");
  }
  Serial.println("\n  Tipo: MIFARE Classic 1K");
  Serial.println("  Capacidad: 1024 bytes, 16 sectores de 4 bloques de 16 bytes cada uno");
}