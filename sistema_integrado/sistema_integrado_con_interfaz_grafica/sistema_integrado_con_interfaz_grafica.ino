#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <FirebaseESP32.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <DHT.h>
#include "HX711.h"

// Configuración de credenciales de WiFi y Firebase
#define WIFI_SSID "Profesores"           // Nombre de la red WiFi
#define WIFI_PASSWORD "einstein"       // Contraseña de la red WiFi
#define FIREBASE_HOST "reactivos-a6fee-default-rtdb.firebaseio.com"  // URL de la base de datos de Firebase
#define FIREBASE_AUTH "DJ0SRNfVwkCtnA0zqxPmWNLEHqRS4r7YdqRg7Rd8"    // Token de autenticación de Firebase
#define DHTPIN 4     // Pin donde está conectado el DHT11
#define DHTTYPE DHT11  // Tipo de sensor DHT

// Inicialización de objetos PN532 y Firebase
PN532_I2C pn532_i2c(Wire);
PN532 nfc(pn532_i2c);
FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000);  // UTC-3 (Paraguay)
DHT dht(DHTPIN, DHTTYPE);
HX711 scale;

// Estructura para almacenar los datos del reactivo
struct Reactivo {
  String producto, numero, alta, marca, codigo, presentacion, lote, vencimiento, baja;
};

// Declaración de la variable para definir el modo de operación
bool interfaceMode = false;  // Modo manual por defecto

// Clave predeterminada para autenticar los bloques en las etiquetas RFID
const uint8_t DEFAULT_KEY[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const int TOTAL_BLOCKS = 63;  // Número total de bloques en la tarjeta MIFARE Classic 1K

// Definicin de Pines de Amplificador de Carga
const int LOADCELL_DOUT_PIN = 18;
const int LOADCELL_SCK_PIN = 19;

// Inicializa la conexión WiFi
void initWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // Conectar a la red WiFi
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) {  // Intentar 10 veces
    Serial.print(".");
    delay(1000);  // Espera entre intentos
    retries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado a WiFi con IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nError: No se pudo conectar a WiFi. Reintentando...");
    delay(5000);  // Espera 5 segundos antes de reintentar
    initWiFi();  // Reintentar conexión
  }
}

// Inicializa la conexión a Firebase
void initFirebase() {
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);  // Permitir reconexión automática

  if (Firebase.ready()) {
    Serial.println("Conexión a Firebase exitosa.");
  } else {
    Serial.println("Error: No se pudo conectar a Firebase. Reintentando...");
    delay(5000);
    initFirebase();  // Reintentar conexión a Firebase
  }
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
  Serial.println("Lector RFID inicializado correctamente.");
}

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
  timeClient.begin();  // Inicializar NTP para obtener la hora
  dht.begin();  // Inicializar el sensor DHT11
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN); // Inicializar amplificador de carga HX711

  // Calibrar celda de carga
  float calibration_factor = 98.16714697406340;  // Ajustar según la calibración
  scale.set_scale(calibration_factor);
  scale.tare();  // Restablecer a cero
  delay(500);  // Esperar 500 ms antes de verificar si el HX711 está listo

  // Verificar que el HX711 esté listo
  if (!scale.is_ready()) {
    Serial.println("No se encontró el amplificador HX711. Verifique las conexiones.");
  } else {
    Serial.println("Amplificador HX711 inicializado correctamente.");
  }

  Serial.printf("Setup completado. Memoria libre: %d bytes\n", ESP.getFreeHeap());
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
      // Notificar si un campo que no es "baja" está vacío
      if (campos[i] == "09_baja" && valores[i]->length() == 0) {
        continue;
      } else if (valores[i]->length() == 0) {
        Serial.println("Valor vacío para " + campos[i] + ", no se enviará a Firebase.");  // No se envía si el valor está vacío y no es "baja"
      }
    }
  }

  if (success) {
    Serial.println("Datos enviados con éxito.\n");  // Todos los datos se enviaron con éxito
  } else {
    Serial.println("Hubo errores al enviar los datos.");  // Algunos datos fallaron al enviarse
  }
}

// Función loop para ejecutar modo interfaz o modo manual
void loop() {

  // Lógica para escuchar comandos de python tkinter
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    // Verificar si el comando es desde la interfaz gráfica (Python/Tkinter)
    if (comando == "WRITE") {
      interfaceMode = true;  // Entrar en modo interfaz
      programarEtiqueta();   // Ejecutar la función para programar la etiqueta
    } else if (comando == "READ") {
      interfaceMode = true;  // Entrar en modo interfaz
      leerEtiqueta();        // Ejecutar la función para leer la etiqueta
    } else if (comando == "TRACK") {
      interfaceMode = true;  // Entrar en modo interfaz
      registrarUso();        // Ejecutar la función para leer etiqueta y registrar uso de reactivo
    } else if (comando == "OUT") {
      interfaceMode = true;  // Entrar en modo interfaz
      registrarBaja();        // Ejecutar la función para registrar la fecha de baja del reactivo
    } else {
      // Si no es un comando de la interfaz, suponer que es una opción del menú manual
      int option = comando.toInt();  // Convertir el comando en una opción del menú
      
      // Validar si la opción del menú es un número válido
      if (option > 0 && option <= 7) {  // Asegurar que sea una opción válida
        interfaceMode = false;         // Entrar en modo manual
        executeMenuOption(option);  // Ejecutar la opción del menú
      } else {
        Serial.println("Opción no válida. Intente nuevamente.");
        showMenu();  // Mostrar el menú nuevamente si la opción no es válida
      }
    }
  } else if (!interfaceMode) {
    // Si no hay comando desde la interfaz y el sistema está en modo manual, mostrar el menú
    showMenu();
    // Esperar una nueva entrada para continuar en el modo manual
    while (!Serial.available()) {
      delay(100);  // Esperar una entrada del usuario
    }
  }
}

/////////////////// MODO INTERFAZ ////////////////////

void leerEtiqueta() {
  Serial.println("Comando 'READ' recibido. Esperando etiqueta...");
  
  uint8_t uid[7];
  uint8_t uidLength;

  // Detectar la etiqueta NFC
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.println("Etiqueta detectada. Verificando campos...");

    // Limpiar el buffer del Serial para evitar residuos
    while (Serial.available()) {
      Serial.read();
    }

    // Obtener el UID como un string para usarlo como clave en Firebase
    String uidString = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      // Hacer que el string sea de 2 dígitos, agregando ceros si es necesario
      if (uid[i] < 0x10) uidString += "0";
      uidString += String(uid[i], HEX);
    }

    // Bloques donde se leerán los valores
    int bloques[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};
    String etiquetas[] = {"Producto", "Número", "Alta", "Marca", "Código", "Presentación", "Lote", "Vencimiento", "Baja"};
    String valores[9];  // Almacenar los valores leídos

    // Leer y almacenar los valores en el array 'valores'
    for (int i = 0; i < 9; i++) {
      if (!leerBloqueGUI(uid, uidLength, bloques[i], valores[i])) {
        Serial.print("Error al leer el bloque ");
        Serial.println(bloques[i]);
        return;
      }
    }

    // Crear un objeto de tipo Reactivo y llenarlo con los datos leídos
    Reactivo reactivo = {valores[0], valores[1], valores[2], valores[3], valores[4], valores[5], valores[6], valores[7], valores[8]};

    // Si el campo "alta" está vacío, registrar la fecha de alta obtenida por NTP
    if (reactivo.alta.length() == 0) {
      timeClient.update();

      // Obtener el tiempo epoch y convertirlo a estructura de tiempo
      time_t rawtime = timeClient.getEpochTime();
      struct tm *tiempoInfo = gmtime(&rawtime);

      // Crear la cadena de fecha formateada (día/mes/año) SIN la hora
      String fechaAlta = String(tiempoInfo->tm_mday) + "/" + String(tiempoInfo->tm_mon + 1) + "/" + String(tiempoInfo->tm_year + 1900);

      uint8_t buffer[16] = {0};
      fechaAlta.getBytes(buffer, 16);
      if (escribirBloqueGUI(uid, uidLength, 6, buffer, "alta")) {
        reactivo.alta = fechaAlta;
        Serial.println();
        leerDatosGUI();
        Serial.println();
        Serial.println("Fecha de alta registrada: " + fechaAlta);
      }
    }

    // Imprimir los valores con el formato solicitado
    Serial.println("Fecha de alta previamente registrada. Leyendo bloques...");
    Serial.println("\nDatos del Reactivo:");
    Serial.print("UID: ");
    Serial.println(uidString);
    for (int i = 0; i < 9; i++) {
      if (etiquetas[i] == "Baja" && valores[i].length() == 0) {
        continue;  // Saltar mensaje para "baja" cuando está vacío
      }
      Serial.print(etiquetas[i]);
      Serial.print(": ");
      if (valores[i].length() > 0) {
        Serial.println(valores[i]);
      } else {
        Serial.println("Sin valor");  // Opcional: indicar campos vacíos, excluyendo "baja"
      }
    } 

    Serial.print("Creando la ruta en Firebase: " + uidString);
    Serial.println("...");

    // Enviar los datos a Firebase
    enviarDatosAFirebase(reactivo, uidString);
    Serial.println("Lectura completa.");
  } else {
    Serial.println("No se detectó ninguna etiqueta.");
  }
}

// Función para leer etiqueta sin enviar datos a Firebase
void leerDatosGUI() { 
  uint8_t uid[7];
  uint8_t uidLength;

  // Detectar la etiqueta NFC
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.println("Leyendo bloques...");

    // Limpiar el buffer del Serial para evitar residuos
    while (Serial.available()) {
      Serial.read();
    }

    // Verificar longitud del UID
    if (uidLength == 0 || uidLength > 7) {
      Serial.println("Error: UID inválido.");
      return;
    }

    // Obtener el UID como un string para usarlo como clave en Firebase
    String uidString = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      // Hacer que el string sea de 2 dígitos, agregando ceros si es necesario
      if (uid[i] < 0x10) uidString += "0";
      uidString += String(uid[i], HEX);
    }

    // Bloques donde se leerán los valores
    int bloques[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};
    String etiquetas[] = {"Producto", "Número", "Alta", "Marca", "Código", "Presentación", "Lote", "Vencimiento", "Baja"};
    String valores[9];  // Almacenar los valores leídos

    // Leer y almacenar los valores en el array 'valores'
    for (int i = 0; i < 9; i++) {
      if (!leerBloqueGUI(uid, uidLength, bloques[i], valores[i])) {
        Serial.print("Error al leer el bloque ");
        Serial.println(bloques[i]);
        return;
      }
    }

    // Crear un objeto de tipo Reactivo y llenarlo con los datos leídos
    Reactivo reactivo = {valores[0], valores[1], valores[2], valores[3], valores[4], valores[5], valores[6], valores[7], valores[8]};

    // Imprimir los valores con el formato solicitado
    Serial.println("\nDatos del Reactivo:");
    Serial.print("UID: ");
    Serial.println(uidString);
    for (int i = 0; i < 9; i++) {
      if (etiquetas[i] == "Baja" && valores[i].length() == 0) {
        continue;  // Saltar mensaje para "baja" cuando está vacío
      }
      Serial.print(etiquetas[i]);
      Serial.print(": ");
      if (valores[i].length() > 0) {
        Serial.println(valores[i]);
      } else {
        Serial.println("Sin valor");  // Opcional: indicar campos vacíos, excluyendo "baja"
      }
    } 
    Serial.println("Lectura completa.");
  } else {
    Serial.println("No se detectó ninguna etiqueta.");
  }
}

// Función para registrar uso y mostrar los datos en el formato solicitado
void registrarUso() {
  Serial.println("Comando 'TRACK' recibido. Esperando etiqueta...");

  uint8_t uid[7];
  uint8_t uidLength;

  // Detectar la etiqueta NFC
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.println("No se detectó ninguna etiqueta.");
    return;
  }

  Serial.println("Etiqueta detectada. Leyendo bloques...");

  // Limpiar el buffer del Serial para evitar residuos
  while (Serial.available()) {
    Serial.read();
  }

  // Verificar el UID después de la lectura
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

  // Bloques donde se leerán los valores
  int bloques[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};
    String etiquetas[] = {"Producto", "Número", "Alta", "Marca", "Código", "Presentación", "Lote", "Vencimiento", "Baja"};
    String valores[9];  // Almacenar los valores leídos

  // Leer y almacenar los valores en el array 'valores'
  for (int i = 0; i < 9; i++) {
    if (!leerBloqueGUI(uid, uidLength, bloques[i], valores[i])) {
      Serial.print("Error al leer el bloque ");
      Serial.println(bloques[i]);
      return;
    }
  }

  // Crear un objeto de tipo Reactivo y llenarlo con los datos leídos
  Reactivo reactivo = {valores[0], valores[1], valores[2], valores[3], valores[4], valores[5], valores[6], valores[7], valores[8]};

  // Verificar si ya tiene un alta
  if (reactivo.alta.length() == 0) {
    // Si no hay alta, escribir la fecha actual obtenida por NTP
    timeClient.update();

    // Obtener el tiempo epoch y convertirlo a estructura de tiempo
    time_t rawtime = timeClient.getEpochTime();
    struct tm *tiempoInfo = gmtime(&rawtime);

    // Crear la cadena de fecha formateada (día/mes/año) SIN la hora
    String fechaAlta = String(tiempoInfo->tm_mday) + "/" + String(tiempoInfo->tm_mon + 1) + "/" + String(tiempoInfo->tm_year + 1900);

    uint8_t buffer[16] = {0};
    fechaAlta.getBytes(buffer, 16);
    escribirBloqueGUI(uid, uidLength, 6, buffer, "alta");
    reactivo.alta = fechaAlta;
  }

  // Imprimir los valores con el formato solicitado
    Serial.println("\nDatos del Reactivo:");
    Serial.print("UID: ");
    Serial.println(uidString);
    for (int i = 0; i < 9; i++) {
      if (etiquetas[i] == "Baja" && valores[i].length() == 0) {
        continue;  // Saltar mensaje para "baja" cuando está vacío
      }
      Serial.print(etiquetas[i]);
      Serial.print(": ");
      if (valores[i].length() > 0) {
        Serial.println(valores[i]);
      } else {
        Serial.println("Sin valor");  // Opcional: indicar campos vacíos, excluyendo "baja"
      }
    } 
    
    // Leer datos del sensor DHT11
    float temperatura = dht.readTemperature();
    float humedad = dht.readHumidity();

    if (isnan(temperatura) || isnan(humedad)) {
      Serial.println("Error al leer datos del sensor DHT11!");
    } else {
      Serial.print("Temperatura: ");
      Serial.print(temperatura);
      Serial.println(" °C");
      Serial.print("Humedad: ");
      Serial.print(humedad);
      Serial.println(" %");
    }
    
    // Leer sensor HX711
    float peso = 0;
    if (scale.is_ready()) {
      peso = scale.get_units(20);  // Obtener el promedio de 10 lecturas
      Serial.print("Peso: ");
      Serial.print(peso);  // Imprimir el peso con 2 decimales
      Serial.println(" g");
    } else {
      Serial.println("Error: El HX711 no está listo. Reintentar...");
      delay(100);  // Retraso antes de reintentar
    }
    
    // Crear la ruta usando el UID como clave
    String path = uidString;

    Serial.println();
    Serial.print("Creando la ruta en Firebase: " + uidString);
    Serial.println("...");

    // Enviar los datos a Firebase
    enviarDatosAFirebase(reactivo, path);

    // Leer la fecha y hora actual (uso)
    timeClient.update();
    time_t rawtime = timeClient.getEpochTime();
    struct tm *tiempoInfo = gmtime(&rawtime);
    String fechaUso = String(tiempoInfo->tm_mday) + "/" + String(tiempoInfo->tm_mon + 1) + "/" + String(tiempoInfo->tm_year + 1900);
    String horaUso = String(tiempoInfo->tm_hour) + ":" + String(tiempoInfo->tm_min) + ":" + String(tiempoInfo->tm_sec);

    // Generar un timestamp único para registrar el uso
    String timestamp = String(rawtime);  // Se declara la variable timestamp

    // Crear una nueva entrada en Firebase con la fecha, hora, temperatura y humedad
    String usoPath = "/reactivos/" + path + "/Registros de Uso/" + timestamp;
    if (!Firebase.setString(firebaseData, usoPath + "/fecha_uso", fechaUso)) {
      Serial.println("Error al enviar la fecha de uso: " + firebaseData.errorReason());
    }

    if (!Firebase.setString(firebaseData, usoPath + "/hora_uso", horaUso)) {
      Serial.println("Error al enviar la hora de uso: " + firebaseData.errorReason());
    }

    if (!Firebase.setFloat(firebaseData, usoPath + "/temperatura", temperatura)) {
      Serial.println("Error al enviar la temperatura: " + firebaseData.errorReason());
    }

    if (!Firebase.setFloat(firebaseData, usoPath + "/humedad", humedad)) {
      Serial.println("Error al enviar la humedad: " + firebaseData.errorReason());
    }
    if (!Firebase.setFloat(firebaseData, usoPath + "/peso", peso)) {
      Serial.println("Error al enviar el peso: " + firebaseData.errorReason());
      } else {
    Serial.println("Registro de uso guardado en la nube.");
  }
}

// Función para registrar fecha de baja cuando se acaba un reactivo
void registrarBaja() {
  Serial.println("Comando 'OUT' recibido. Esperando etiqueta...");

  uint8_t uid[7];
  uint8_t uidLength;

  // Detectar la etiqueta NFC
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.println("Etiqueta detectada. Verificando campos...");

    // Limpiar el buffer del Serial para evitar residuos
    while (Serial.available()) {
      Serial.read();
    }

    // Obtener el UID como un string para usarlo como clave en Firebase
    String uidString = "";
      for (uint8_t i = 0; i < uidLength; i++) {
        // Hacer que el string sea de 2 dígitos, agregando ceros si es necesario
        if (uid[i] < 0x10) uidString += "0";
        uidString += String(uid[i], HEX);
    }

    // Bloques donde se leerán los valores
    int bloques[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};
      String etiquetas[] = {"Producto", "Número", "Alta", "Marca", "Código", "Presentación", "Lote", "Vencimiento", "Baja"};
      String valores[9];  // Almacenar los valores leídos

    // Leer y almacenar los valores en el array 'valores'
    for (int i = 0; i < 9; i++) {
      if (!leerBloqueGUI(uid, uidLength, bloques[i], valores[i])) {
        Serial.print("Error al leer el bloque ");
        Serial.println(bloques[i]);
        return;
      }
    }

    // Crear un objeto de tipo Reactivo y llenarlo con los datos leídos
    Reactivo reactivo = {valores[0], valores[1], valores[2], valores[3], valores[4], valores[5], valores[6], valores[7], valores[8]};

    // Verificar si ya tiene una fecha de baja
    if (reactivo.baja.length() == 0) {
      // Si no hay alta, escribir la fecha actual obtenida por NTP
      timeClient.update();

      // Obtener el tiempo epoch y convertirlo a estructura de tiempo
      time_t rawtime = timeClient.getEpochTime();
      struct tm *tiempoInfo = gmtime(&rawtime);

      // Crear la cadena de fecha formateada (día/mes/año) SIN la hora
      String fechaBaja = String(tiempoInfo->tm_mday) + "/" + String(tiempoInfo->tm_mon + 1) + "/" + String(tiempoInfo->tm_year + 1900);

      uint8_t buffer[16] = {0};
      fechaBaja.getBytes(buffer, 16);
      // Escribir la fecha en el bloque 14
      if (escribirBloqueGUI(uid, uidLength, 14, buffer, "baja")) {
        reactivo.baja = fechaBaja;  // Actualizar el campo "baja" en el objeto
        Serial.println();
        leerDatosGUI();
        Serial.println();
        Serial.println("Fecha de baja registrada: " + fechaBaja);
      }
    } else{
    
      Serial.println("Fecha de baja previamente registrada. Leyendo bloques...");
      // Imprimir los valores con el formato solicitado
      Serial.println("\nDatos del reactivo:");
      Serial.print("UID: ");
      Serial.println(uidString);
      for (int i = 0; i < 9; i++) {
        Serial.print(etiquetas[i]);
        Serial.print(": ");
        Serial.println(valores[i]);
      }
    }


    Serial.println();
    Serial.print("Creando la ruta en Firebase: " + uidString);
    Serial.println("...");

    // Enviar los datos a Firebase
    enviarDatosAFirebase(reactivo, uidString);
    Serial.println("Lectura completa.");
  } else{
    Serial.println("No se detectó ninguna etiqueta.");
  }
}


// Función para leer un bloque de datos de la etiqueta NFC
bool leerBloqueGUI(uint8_t* uid, uint8_t uidLength, int block, String &destino) {
  uint8_t buffer[16];  // Buffer para almacenar los datos leídos
  int intentos = 0;
  bool autenticado = false;
  
  // Intentar autenticar hasta 3 veces
  while (intentos < 3 && !autenticado) {
    if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, 0, const_cast<uint8_t*>(DEFAULT_KEY))) {
      autenticado = true;
    } else {
      Serial.print("Error de autenticación en el bloque ");
      Serial.println(block);
      intentos++;
      delay(100); // Esperar antes de reintentar
    }
  }
  
  // Si la autenticación falla después de 3 intentos
  if (!autenticado) {
    Serial.println("Autenticación fallida después de varios intentos.");
    return false;
  }
  
  // Leer datos del bloque
  if (nfc.mifareclassic_ReadDataBlock(block, buffer)) {
    destino = String((char*)buffer).substring(0, 16);  // Convierte los datos en una cadena de caracteres
    destino.trim();  // Elimina espacios en blanco al inicio y final
    return true;
  } else {
    Serial.print("Error al leer el bloque ");
    Serial.println(block);
    return false;
  }
}


// Función para programar una etiqueta NFC en modo interfaz
void programarEtiqueta() {
  Serial.println("Comando 'WRITE' recibido. Esperando datos...");
  
  uint8_t uid[7];
  uint8_t uidLength;

  // Detectar la etiqueta NFC
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.println("Etiqueta detectada. Esperando valores...");
    
    // Esperar a que lleguen los datos concatenados desde Python
    while (!Serial.available()) delay(10);  
    String datos = Serial.readStringUntil('\n');
    datos.trim();

    // Limpiar el buffer del Serial para evitar caracteres residuales
    while (Serial.available()) {
      Serial.read();
    }

    // Verificar los datos recibidos
    Serial.print("Datos recibidos: ");
    Serial.println(datos);

    // Separar los datos recibidos por comas
    String valores[7];
    int index = 0;
    for (int i = 0; i < 7 && index != -1; i++) {
      index = datos.indexOf(',');
      if (index != -1) {
        valores[i] = datos.substring(0, index);
        datos = datos.substring(index + 1);
      } else {
        valores[i] = datos;  // Último valor
      }

      // Verificar la longitud de los datos (no deben exceder 16 caracteres)
      if (valores[i].length() > 16) {
        Serial.print("Error: El valor ");
        Serial.print(i);
        Serial.println(" excede el límite de 16 caracteres.");
        return;
      }

      Serial.print("Valor ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(valores[i]);
    }

    // Bloques donde se guardarán los valores
    int bloques[] = {4, 5, 8, 9, 10, 12, 13};

    // Guardar cada valor en su bloque correspondiente
    for (int i = 0; i < 7; i++) {
      if (valores[i].length() > 0) {
        uint8_t buffer[16] = {0};
        valores[i].getBytes(buffer, 16);  // Convertir el valor en bytes
        if (!escribirBloqueGUI(uid, uidLength, bloques[i], buffer, "valor")) {
          Serial.println("Error al programar la etiqueta.");
          return;
        }
      }
    }

    // Confirmar que los datos han sido guardados
    Serial.println("Datos guardados exitosamente.");
  } else {
    Serial.println("No se detectó ninguna etiqueta.");
  }
}

// Función para escribir en un bloque de la etiqueta NFC
bool escribirBloqueGUI(uint8_t* uid, uint8_t uidLength, int block, uint8_t* buffer, const char* titulo) {
  int intentos = 0;
  bool autenticado = false;

  // Intentar autenticar hasta 3 veces
  while (intentos < 3 && !autenticado) {
    if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, 0, const_cast<uint8_t*>(DEFAULT_KEY))) {
      autenticado = true;
    } else {
      Serial.print("Error de autenticación en el bloque ");
      Serial.println(block);
      intentos++;
      delay(100); // Esperar antes de reintentar
    }
  }

  // Si la autenticación falla después de 3 intentos
  if (!autenticado) {
    Serial.println("Autenticación fallida después de varios intentos.");
    return false;
  }
  
  // Escribir datos en el bloque
  if (nfc.mifareclassic_WriteDataBlock(block, buffer)) {
    Serial.print("Campo '");
    Serial.print(titulo);
    Serial.println("' registrado con éxito!");
    Serial.println();
    return true;
  } else {
    Serial.print("Error al escribir el bloque ");
    Serial.println(block);
    return false;
  }
}



/////////////////// MODO MANUAL ////////////////////

// Función para mostrar menú principal en modo manual
void showMenu() {
  Serial.println("\nPosicione el contenedor en la balanza y acerque una etiqueta MIFARE Classic 1K al lector...");
  Serial.println("\nMenú Principal:");
  Serial.println("1. Leer datos");
  Serial.println("2. Ingresar datos");
  Serial.println("3. Editar datos");
  Serial.println("4. Formatear etiqueta");
  Serial.println("5. Leer todos los sectores");
  Serial.println("6. Mostrar información");
  Serial.println("7. Salir");
  Serial.print("\nSeleccione una opción: ");
}

// Función para ejecutar menú principal en modo manual
void executeMenuOption(int option) {
  uint8_t uid[7];
  uint8_t uidLength;

  if (option != 7) {
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
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
  }

  if (!interfaceMode) {
    Serial.println("Presione cualquier tecla para continuar...");
    while (!Serial.available()) {
      delay(100);
    }
    while (Serial.available()) {
      Serial.read();  // Limpiar el buffer del Serial
    }
    delay(1000);
  }
}


// Función para autenticar y leer un bloque NFC con reintentos
void leerDatosReactivo(uint8_t* uid, uint8_t uidLength) {
  // Detectar la etiqueta NFC
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.println("No se detectó ninguna etiqueta.");
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

  // Verificar si ya tiene un 'alta'
  if (leerBloque(uid, uidLength, 6, reactivo.alta) && reactivo.alta.length() == 0) {
    // Si no hay 'alta', escribir la fecha actual obtenida por NTP
    timeClient.update();

    // Obtener el tiempo epoch y convertirlo a estructura de tiempo
    time_t rawtime = timeClient.getEpochTime();
    struct tm *tiempoInfo = gmtime(&rawtime);

    // Crear la cadena de fecha formateada (día/mes/año) SIN la hora
    String fechaAlta = String(tiempoInfo->tm_mday) + "/" + String(tiempoInfo->tm_mon + 1) + "/" + String(tiempoInfo->tm_year + 1900);

    uint8_t buffer[16] = {0};
    fechaAlta.getBytes(buffer, 16);
    escribirBloque(uid, uidLength, 6, buffer, "alta");
    reactivo.alta = fechaAlta;
    Serial.println("Fecha de alta escrita: " + fechaAlta);
  }

  // Imprimir datos del Reactivo
  Serial.println("\nDatos del Reactivo:");
  for (int i = 0; i < 9; i++) {
    if (leerBloque(uid, uidLength, bloques[i], *campos[i])) {
      Serial.println(campos[i]->c_str());  // Imprime el campo leído
    } else {
      Serial.println("Error leyendo bloque " + String(bloques[i]));
    }
  }

  // Leer datos del sensor DHT11
  float temperatura = dht.readTemperature();
  float humedad = dht.readHumidity();

  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("Error al leer datos del sensor DHT11!");
  } else {
    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" °C");
    Serial.print("Humedad: ");
    Serial.print(humedad);
    Serial.println(" %");
  }

  // Leer sensor HX711
  float peso = 0;
  if (scale.is_ready()) {
    peso = scale.get_units(20);  // Obtener el promedio de 10 lecturas
    Serial.print("Peso: ");
    Serial.print(peso);  // Imprimir el peso con 2 decimales
    Serial.println(" g");
  } else {
    Serial.println("Error: El HX711 no está listo. Reintentar...");
    delay(100);  // Retraso antes de reintentar
  }

  // Crear la ruta usando el UID como clave
  String path = uidString;

  Serial.println();
  Serial.print("Creando la ruta en Firebase: " + path);
  Serial.println("...");

  // Enviar datos a Firebase
  Serial.println();
  Serial.println("Enviando datos...");
  enviarDatosAFirebase(reactivo, path);

  // Leer la fecha y hora actual (uso)
  timeClient.update();
  time_t rawtime = timeClient.getEpochTime();
  struct tm *tiempoInfo = gmtime(&rawtime);
  String fechaUso = String(tiempoInfo->tm_mday) + "/" + String(tiempoInfo->tm_mon + 1) + "/" + String(tiempoInfo->tm_year + 1900);
  String horaUso = String(tiempoInfo->tm_hour) + ":" + String(tiempoInfo->tm_min) + ":" + String(tiempoInfo->tm_sec);

  // Generar un timestamp único para registrar el uso
  String timestamp = String(rawtime);  // Se declara la variable timestamp

  // Crear una nueva entrada en Firebase con la fecha, hora, temperatura y humedad
  String usoPath = "/reactivos/" + path + "/Registros de Uso/" + timestamp;
  if (!Firebase.setString(firebaseData, usoPath + "/fecha_uso", fechaUso)) {
    Serial.println("Error al enviar la fecha de uso: " + firebaseData.errorReason());
  }

  if (!Firebase.setString(firebaseData, usoPath + "/hora_uso", horaUso)) {
    Serial.println("Error al enviar la hora de uso: " + firebaseData.errorReason());
  }

  if (!Firebase.setFloat(firebaseData, usoPath + "/temperatura", temperatura)) {
    Serial.println("Error al enviar la temperatura: " + firebaseData.errorReason());
  }

  if (!Firebase.setFloat(firebaseData, usoPath + "/humedad", humedad)) {
    Serial.println("Error al enviar la humedad: " + firebaseData.errorReason());
  }

  if (!Firebase.setFloat(firebaseData, usoPath + "/peso", peso)) {
    Serial.println("Error al enviar el peso: " + firebaseData.errorReason());
  }

  Serial.println("Uso registrado correctamente en Firebase.");
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
    if (i == 2) continue;  // Saltar el campo "alta" (bloque 6)
    
    Serial.print("Ingrese ");
    Serial.print(campos[i]);
    Serial.println(" (máx. 16 caracteres):");
    
    while (!Serial.available()) delay(10);
    String dato = Serial.readStringUntil('\n');  // Lee la entrada del usuario
    dato.trim();

    // Limpiar el buffer del Serial para evitar residuos
    while (Serial.available()) {
      Serial.read();
    }

    // Verificar la longitud de los datos (no deben exceder 16 caracteres)
    if (dato.length() > 16) {
      Serial.print("Error: El valor para ");
      Serial.print(campos[i]);
      Serial.println(" excede el límite de 16 caracteres. Ingrese nuevamente:");
      dato = Serial.readStringUntil('\n');
      dato.trim();
    }
    
    uint8_t buffer[16] = {0};  // Almacena los datos ingresados
    dato.getBytes(buffer, 16);
    escribirBloque(uid, uidLength, bloques[i], buffer, campos[i].c_str());  // Escribe los datos en el bloque correspondiente
  }
}

// Función para leer un bloque de datos de la etiqueta
bool leerBloque(uint8_t* uid, uint8_t uidLength, int block, String &destino) {
  uint8_t buffer[16];  // Buffer para almacenar los datos leídos
  int intentos = 0;
  bool autenticado = false;
  
  // Intentar autenticar hasta 3 veces
  while (intentos < 3 && !autenticado) {
    if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, 0, const_cast<uint8_t*>(DEFAULT_KEY))) {
      autenticado = true;
    } else {
      Serial.print("Error de autenticación en el bloque ");
      Serial.println(block);
      intentos++;
      delay(100); // Esperar antes de reintentar
    }
  }
  
  // Si la autenticación falla después de 3 intentos
  if (!autenticado) {
    Serial.println("Autenticación fallida después de varios intentos.");
    return false;
  }
  
  // Leer datos del bloque
  if (nfc.mifareclassic_ReadDataBlock(block, buffer)) {
    destino = String((char*)buffer).substring(0, 16);  // Convierte los datos en una cadena de caracteres
    destino.trim();  // Elimina espacios en blanco al inicio y final
    return true;
  } else {
    Serial.print("Error al leer el bloque ");
    Serial.println(block);
    return false;
  }
}

// Función para autenticar y escribir en un bloque NFC con reintentos
void escribirBloque(uint8_t* uid, uint8_t uidLength, int block, uint8_t* buffer, const char* titulo) {
  int intentos = 0;
  bool autenticado = false;

  // Intentar autenticar hasta 3 veces
  while (intentos < 3 && !autenticado) {
    if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, 0, const_cast<uint8_t*>(DEFAULT_KEY))) {
      autenticado = true;
    } else {
      Serial.print("Error de autenticación en el bloque ");
      Serial.println(block);
      intentos++;
      delay(100); // Esperar antes de reintentar
    }
  }

  // Si la autenticación falla después de 3 intentos
  if (!autenticado) {
    Serial.println("Autenticación fallida después de varios intentos.");
    return;
  }
  
  // Escribir datos en el bloque
  if (nfc.mifareclassic_WriteDataBlock(block, buffer)) {
    
    Serial.print(titulo);
    Serial.println(" editado exitosamente!");
    Serial.println();
  } else {
    Serial.print("Error al escribir el bloque ");
    Serial.println(block);
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

// Función para formatear uno o todos los bloques específicos de la tarjeta NFC
void formatCard(uint8_t* uid, uint8_t uidLength) {
  // Definir los campos y bloques correspondientes
  String campos[] = {"producto", "numero", "alta", "marca", "codigo", "presentacion", "lote", "vencimiento", "baja"};
  int bloques[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};

  Serial.println("\n¿Desea formatear un solo campo o todos los campos?");
  Serial.println("1. Formatear un solo campo");
  Serial.println("2. Formatear todos los campos");

  // Esperar la selección del usuario
  while (!Serial.available()) {
    delay(100);
  }
  int opcion = Serial.parseInt();

  // Limpiar el buffer del Serial para evitar caracteres residuales
  while (Serial.available()) {
    Serial.read();
  }

  if (opcion == 1) {
    // Formatear un solo campo
    Serial.println("\n¿Qué campo desea formatear?");
    for (int i = 0; i < 9; i++) {
      Serial.printf("%d. %s\n", i + 1, campos[i].c_str());
    }

    // Esperar la selección del usuario
    while (!Serial.available()) {
      delay(100);
    }
    int seleccion = Serial.parseInt();
    if (seleccion < 1 || seleccion > 9) {
      Serial.println("Selección inválida.");
      return;
    }

    // Limpiar el buffer del Serial para evitar caracteres residuales
    while (Serial.available()) {
      Serial.read();
    }

    // Obtener el bloque correspondiente a la selección del usuario
    int bloqueSeleccionado = bloques[seleccion - 1];

    // Confirmar con el usuario si realmente desea formatear el bloque
    Serial.print("¿Está seguro que desea formatear el bloque para el campo '");
    Serial.print(campos[seleccion - 1]);
    Serial.println("'? (S/N):");

    // Esperar confirmación del usuario
    while (!Serial.available()) {
      delay(100);
    }
    char confirmacion = Serial.read();
    
    // Limpiar nuevamente el buffer por si hay un carácter de nueva línea o retorno
    while (Serial.available()) {
      Serial.read();
    }

    if (confirmacion != 'S' && confirmacion != 's') {
      Serial.println("Formateo cancelado.");
      return;
    }

    // Proceder a formatear (borrar) el bloque
    uint8_t emptyBlock[16] = {0};  // Bloque vacío para sobrescribir la tarjeta
    if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, bloqueSeleccionado, 0, const_cast<uint8_t*>(DEFAULT_KEY))) {
      if (nfc.mifareclassic_WriteDataBlock(bloqueSeleccionado, emptyBlock)) {
        Serial.print("El bloque del campo '");
        Serial.print(campos[seleccion - 1]);
        Serial.println("' ha sido formateado con éxito.");
      } else {
        Serial.println("Error al formatear el bloque.");
      }
    } else {
      Serial.println("Error de autenticación en el bloque.");
    }
  } else if (opcion == 2) {
    // Formatear todos los campos
    Serial.println("¿Está seguro que desea formatear todos los campos? (S/N):");
    
    // Esperar confirmación del usuario
    while (!Serial.available()) {
      delay(100);
    }
    char confirmacion = Serial.read();
    
    // Limpiar el buffer del Serial
    while (Serial.available()) {
      Serial.read();
    }

    if (confirmacion != 'S' && confirmacion != 's') {
      Serial.println("Formateo cancelado.");
      return;
    }

    // Proceder a formatear todos los bloques
    uint8_t emptyBlock[16] = {0};  // Bloque vacío para sobrescribir la tarjeta
    for (int i = 0; i < 9; i++) {
      if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, bloques[i], 0, const_cast<uint8_t*>(DEFAULT_KEY))) {
        if (nfc.mifareclassic_WriteDataBlock(bloques[i], emptyBlock)) {
          Serial.print("El bloque del campo '");
          Serial.print(campos[i]);
          Serial.println("' ha sido formateado con éxito.");
        } else {
          Serial.println("Error al formatear el bloque.");
        }
      } else {
        Serial.println("Error de autenticación en el bloque.");
      }
    }
  } else {
    Serial.println("Opción inválida.");
  }
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