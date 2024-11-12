// Etapa 2: Conexion del ESP32 con Red WiFi

#include <WiFi.h>   // Librería para establecer conexión WiFi

// Credenciales de red WiFi
const char* ssid = "NombredeRed";      // Escribir nombre de la Red WiFi
const char* password = "Contraseña";  // Escribir contraseña de la Red WiFi

// Tiempo máximo de espera (en milisegundos)
unsigned long timeout = 10000;  // 10 segundos

void setup() {
  // Iniciar comunicación serial
  Serial.begin(115200);
  delay(1000);
  
  // Intentar conectar a la red WiFi
  Serial.println("Conectando a la red WiFi...");
  WiFi.begin(ssid, password);

  // Registrar el tiempo de inicio
  unsigned long startAttemptTime = millis();
  
  // Mantener intentado conectar mientras el tiempo no supere el límite
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    delay(500);
    Serial.print(".");
  }
  
  // Verificar si la conexión fue exitosa
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("Conectado a la red WiFi.");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("No se pudo conectar a la red WiFi.");
  }
}

void loop() {
  // El bucle permanece vacío ya que solo se comprueba la conexión WiFi
}
