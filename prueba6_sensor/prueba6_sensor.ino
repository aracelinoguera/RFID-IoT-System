// Etapa 6: Conexión del ESP32 con el Sensor DHT11

#include <DHT.h>  // Incluir librería para manejar el sensor DHT

// Definir pin de datos y el tipo de sensor (DHT11)
#define DHTPIN 4  // Pin de datos conectado al DHT11
#define DHTTYPE DHT11  // Tipo de sensor: DHT11

// Crear objeto de clase DHT para gestionar el sensor
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);  // Inicializar comunicación serial
  dht.begin();  // Iniciar comunicación con sensor DHT11
  Serial.println("Sensor DHT11 iniciado");  // Confirmación de que el sensor está listo
}

void loop() {
  // Esperar 2 segundos entre cada lectura para evitar saturar el sensor
  delay(2000);

  // Leer la temperatura del sensor DHT11
  float temperatura = dht.readTemperature();
  // Leer la humedad del sensor DHT11
  float humedad = dht.readHumidity();

  // Verificar si las lecturas son válidas
  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("Error al leer el sensor DHT11");  // Imprimir un mensaje de error si no se puede leer el sensor
    return;  // Detener la ejecución hasta la siguiente iteración
  }

  // Mostrar los valores de temperatura y humedad en el monitor serial
  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");
  
  Serial.print("Humedad: ");
  Serial.print(humedad);
  Serial.println(" %");
}
