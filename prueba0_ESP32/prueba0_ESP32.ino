// Etapa 0: Prueba de Conexión del ESP32 Dev Module

// Definir el pin del LED
int ledPin = 2;  // Asignar el pin GPIO 2 al LED

void setup() {
  // Configurar el pin como salida para controlar el LED
  pinMode(ledPin, OUTPUT); 
}

void loop() {
  // Encender el LED enviando una señal alta al pin GPIO 2
  digitalWrite(ledPin, HIGH);  // Encender LED
  delay(1000);  // Esperar 1 segundo con el LED encendido

  // Apagar el LED enviando una señal baja al pin GPIO 2
  digitalWrite(ledPin, LOW);  // Cortar suministro de corriente y apagar el LED.
  delay(1000);  // Esperar 1 segundo con el LED apagado
}
