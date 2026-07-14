#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// =================================================================
// CONFIGURACIÓN DE PANTALLA LCD (I2C)
// =================================================================
// Dirección I2C: 0x27 o 0x3F. Dimensiones: 16 columnas, 2 filas.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =================================================================
// CONFIGURACIÓN DE SENSORES
// =================================================================
// Sensor DHT11 (Temperatura y Humedad Ambiente)
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Sensor de Humedad de Suelo (Analógico)
#define PIN_SUELO 34
// Calibración del sensor de suelo para ESP32 (ADC de 12 bits: 0 - 4095)
const int VALOR_SECO = 3500;  // Lectura del sensor al aire libre (seco)
const int VALOR_HUMEDO = 1200; // Lectura del sensor sumergido en agua (húmedo)

// Variables para almacenar lecturas
float tempAmbiente = 0.0;
float humAmbiente = 0.0;
int humSuelo = 0;

// =================================================================
// CONFIGURACIÓN DE RELÉS (ACTUADORES)
// =================================================================
const int pinesRele[] = {13, 32, 27, 14};
const char* nombres[] = {"LUZ", "VENTILADOR", "BOMBA", "HUMIDIFICADOR"};

// Índices para legibilidad del código
#define RELE_LUZ 0
#define RELE_VENTILADOR 1
#define RELE_BOMBA 2
#define RELE_HUMIDIFICADOR 3

// =================================================================
// PARÁMETROS DE DISEÑO AGRÓNOMO (LENTEJAS)
// =================================================================
// Límites de Temperatura Ambiente (°C)
const float TEMP_MAXIMA = 25.0; // Encender ventilador si supera esto
const float TEMP_OK = 24.0;     // Apagar ventilador cuando baje a esto (Histéresis)

// Límites de Humedad del Aire (%)
const float HUM_AIRE_MAXIMA = 80.0; // Encender ventilador para ventilar si supera esto
const float HUM_AIRE_MINIMA = 65.0; // Encender humidificador si baja de esto
const float HUM_AIRE_OK_ALTA = 75.0; // Apagar ventilador cuando baje de esto
const float HUM_AIRE_OK_BAJA = 75.0; // Apagar humidificador cuando suba a esto

// Límites de Humedad de Suelo (%)
const int HUM_SUELO_MINIMA = 60; // Encender bomba de agua si cae por debajo de esto

// =================================================================
// TEMPORIZADORES Y TIEMPOS DE SEGURIDAD
// =================================================================
// Tiempos para la Bomba de Agua
const unsigned long DURACION_RIEGO = 3000;              // Riego por 3 segundos (3000 ms)
const unsigned long BLOQUEO_RIEGO = 15UL * 60UL * 1000UL; // Bloqueo de 15 minutos (evita inundar)

// Tiempos para el Fotoperíodo de la Luz (14 horas encendido, 10 horas apagado)
// NOTA: Para pruebas rápidas en el laboratorio, puedes cambiar 'UL * 60UL * 60UL * 1000UL'
// por 'UL * 1000UL' para simular en segundos.
const unsigned long TIEMPO_LUZ_ON = 14UL * 60UL * 60UL * 1000UL;
const unsigned long TIEMPO_LUZ_OFF = 10UL * 60UL * 60UL * 1000UL;

// Frecuencias de actualización del sistema
unsigned long ultimoLecturaSensores = 0;
const unsigned long intervaloSensores = 1000; // Leer sensores cada 1 segundo

unsigned long ultimoCambioLCD = 0;
const unsigned long tiempoPaginaLCD = 3000;   // Alternar pantalla cada 3 segundos

// =================================================================
// VARIABLES DE ESTADO
// =================================================================
bool luzEstado = true;             // Inicia encendida para el fotoperíodo
unsigned long ultimoCambioLuz = 0;

bool bombaRegando = false;
unsigned long inicioRiego = 0;
unsigned long ultimoRiego = 0;

bool ventiladorEstado = false;
bool humidificadorEstado = false;

int paginaLCD = 0;                 // 0 = Sensores, 1 = Actuadores
bool debeActualizarLCD = true;

// =================================================================
// FUNCIÓN AUXILIAR CONTROL DE RELÉ (Activos en Bajo)
// =================================================================
// true = Encender (envía LOW), false = Apagar (envía HIGH)
void controlRele(int indice, bool encender) {
  digitalWrite(pinesRele[indice], encender ? LOW : HIGH);
}

void setup() {
  Serial.begin(115200);
  
  // Forzar pines I2C para ESP32 (SDA = 21, SCL = 22)
  Wire.begin(21, 22); 
  
  // Inicializar sensor DHT
  dht.begin();
  
  // Configurar pines de relés y apagarlos todos inicialmente
  for (int i = 0; i < 4; i++) {
    pinMode(pinesRele[i], OUTPUT);
    controlRele(i, false); 
  }
  
  // Iniciar la luz del fotoperíodo encendida
  controlRele(RELE_LUZ, luzEstado);
  ultimoCambioLuz = millis();
  
  // Inicializar Pantalla LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Germinadora IA");
  lcd.setCursor(0, 1);
  lcd.print("Modo Lenteja v1");
  delay(2500);
  lcd.clear();
}

void loop() {
  unsigned long tiempoActual = millis();

  // =================================================================
  // 1. LECTURA DE SENSORES EN TIEMPO REAL (Cada 1 segundo)
  // =================================================================
  if (tiempoActual - ultimoLecturaSensores >= intervaloSensores) {
    ultimoLecturaSensores = tiempoActual;
    
    // Leer DHT11
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (!isnan(t)) tempAmbiente = t;
    if (!isnan(h)) humAmbiente = h;
    
    // Leer sensor de suelo
    int lecturaSuelo = analogRead(PIN_SUELO);
    int pctSuelo = map(lecturaSuelo, VALOR_SECO, VALOR_HUMEDO, 0, 100);
    humSuelo = constrain(pctSuelo, 0, 100);
    
    // Solicitar redibujo del LCD al tener nuevos datos
    debeActualizarLCD = true;
  }

  // =================================================================
  // 2. LOGICA DE CONTROL AUTOMÁTICO DE PERIFÉRICOS
  // =================================================================
  
  // A. Control del Fotoperíodo (LUZ)
  unsigned long duracionCicloLuz = luzEstado ? TIEMPO_LUZ_ON : TIEMPO_LUZ_OFF;
  if (tiempoActual - ultimoCambioLuz >= duracionCicloLuz) {
    luzEstado = !luzEstado;
    ultimoCambioLuz = tiempoActual;
    controlRele(RELE_LUZ, luzEstado);
    
    Serial.print("[LUZ] Cambio de fotoperiodo. Estado actual: ");
    Serial.println(luzEstado ? "ENCENDIDO" : "APAGADO");
    debeActualizarLCD = true;
  }

  // B. Control de Humedad del Suelo (BOMBA DE AGUA - RIEGO INTELIGENTE)
  if (bombaRegando) {
    // Si ya está regando, verificar si terminó el pulso de 3 segundos
    if (tiempoActual - inicioRiego >= DURACION_RIEGO) {
      bombaRegando = false;
      ultimoRiego = tiempoActual; // Guarda el momento en que terminó de regar
      controlRele(RELE_BOMBA, false); // Apagar bomba
      
      Serial.println("[BOMBA] Riego terminado. Iniciando 15 min de bloqueo.");
      debeActualizarLCD = true;
    }
  } else {
    // Si no está regando, verificar si el suelo está seco (< 60%)
    // Y comprobar que haya pasado el tiempo de bloqueo (15 minutos) desde el último riego
    if (humSuelo < HUM_SUELO_MINIMA) {
      if (ultimoRiego == 0 || (tiempoActual - ultimoRiego >= BLOQUEO_RIEGO)) {
        bombaRegando = true;
        inicioRiego = tiempoActual;
        controlRele(RELE_BOMBA, true); // Encender bomba
        
        Serial.println("[BOMBA] Humedad baja. Encendiendo bomba por 3 segundos.");
        debeActualizarLCD = true;
      }
    }
  }

  // C. Control del Ventilador (Temperatura / Humedad del aire alta)
  // Implementamos histéresis para evitar clics repetitivos en los relés
  if (!ventiladorEstado) {
    // Condición de encendido
    if (tempAmbiente > TEMP_MAXIMA || humAmbiente > HUM_AIRE_MAXIMA) {
      ventiladorEstado = true;
      controlRele(RELE_VENTILADOR, true);
      
      Serial.print("[VENTILADOR] Activado. T: "); Serial.print(tempAmbiente);
      Serial.print("C | H.A: "); Serial.print(humAmbiente); Serial.println("%");
      debeActualizarLCD = true;
    }
  } else {
    // Condición de apagado (ambos parámetros deben regresar a zona segura)
    if (tempAmbiente < TEMP_OK && humAmbiente < HUM_AIRE_OK_ALTA) {
      ventiladorEstado = false;
      controlRele(RELE_VENTILADOR, false);
      
      Serial.println("[VENTILADOR] Desactivado. Rango seguro alcanzado.");
      debeActualizarLCD = true;
    }
  }

  // D. Control del Humidificador (Humedad del aire baja)
  if (!humidificadorEstado) {
    // Condición de encendido
    if (humAmbiente < HUM_AIRE_MINIMA) {
      humidificadorEstado = true;
      controlRele(RELE_HUMIDIFICADOR, true);
      
      Serial.print("[HUMIDIFICADOR] Activado por baja humedad ambiental: ");
      Serial.print(humAmbiente); Serial.println("%");
      debeActualizarLCD = true;
    }
  } else {
    // Condición de apagado
    if (humAmbiente > HUM_AIRE_OK_BAJA) {
      humidificadorEstado = false;
      controlRele(RELE_HUMIDIFICADOR, false);
      
      Serial.println("[HUMIDIFICADOR] Desactivado. Humedad ambiental recuperada.");
      debeActualizarLCD = true;
    }
  }

  // =================================================================
  // 3. CAMBIO DE PÁGINAS DEL LCD (Cada 3 segundos)
  // =================================================================
  if (tiempoActual - ultimoCambioLCD >= tiempoPaginaLCD) {
    ultimoCambioLCD = tiempoActual;
    paginaLCD = (paginaLCD == 0) ? 1 : 0; // Alterna entre Página 0 y 1
    lcd.clear(); // Limpiar pantalla SOLO al cambiar de página para evitar parpadeos
    debeActualizarLCD = true;
  }

  // =================================================================
  // 4. ACTUALIZACIÓN FÍSICA DEL LCD
  // =================================================================
  if (debeActualizarLCD) {
    actualizarLCD();
    debeActualizarLCD = false;
  }
}

// Escribe una línea completa en el LCD (exactamente 16 caracteres) para evitar parpadeos
void printLineaLCD(int fila, const char* str) {
  char buffer[17];
  int len = strlen(str);
  int i = 0;
  for (i = 0; i < len && i < 16; i++) {
    buffer[i] = str[i];
  }
  for (; i < 16; i++) {
    buffer[i] = ' '; // Rellena el resto con espacios vacíos
  }
  buffer[16] = '\0';
  
  lcd.setCursor(0, fila);
  lcd.print(buffer);
}

// Dibuja en la pantalla LCD según la página activa
void actualizarLCD() {
  char buffer[32];
  
  if (paginaLCD == 0) {
    // ----------------------------------------------------
    // PÁGINA 0: LECTURA DE SENSORES
    // ----------------------------------------------------
    // Fila 0: Temp y Humedad del Aire
    snprintf(buffer, sizeof(buffer), "T:%.1fC  H.A:%d%%", tempAmbiente, (int)humAmbiente);
    printLineaLCD(0, buffer);
    
    // Fila 1: Humedad del Suelo
    snprintf(buffer, sizeof(buffer), "H.Suelo: %d%%", humSuelo);
    printLineaLCD(1, buffer);
    
  } else {
    // ----------------------------------------------------
    // PÁGINA 1: ESTADO DE ACTUADORES (RELÉS)
    // ----------------------------------------------------
    // Fila 0: Luz y Ventilador
    snprintf(buffer, sizeof(buffer), "LUZ:%s  VEN:%s", 
             luzEstado ? "ON" : "OFF", 
             ventiladorEstado ? "ON" : "OFF");
    printLineaLCD(0, buffer);
    
    // Fila 1: Bomba y Humidificador
    snprintf(buffer, sizeof(buffer), "BOM:%s  HUM:%s", 
             bombaRegando ? "ON" : "OFF", 
             humidificadorEstado ? "ON" : "OFF");
    printLineaLCD(1, buffer);
  }
}
