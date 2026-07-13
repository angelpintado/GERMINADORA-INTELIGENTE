#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ==========================================
// CONFIGURACIÓN DE PANTALLA LCD (I2C)
// ==========================================
// Dirección I2C: 0x27 o 0x3F. Dimensiones: 16 columnas, 2 filas.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==========================================
// CONFIGURACIÓN DE SENSORES
// ==========================================
// Sensor DHT11 (Temperatura y Humedad Ambiente)
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Sensor de Humedad de Suelo (Analógico)
#define PIN_SUELO 34
// Calibración del sensor de suelo para ESP32 (ADC de 12 bits: 0 - 4095)
// Nota: Ajusta estos valores según tus lecturas reales si es necesario.
const int VALOR_SECO = 3500;  // Lectura del sensor al aire libre (seco)
const int VALOR_HUMEDO = 1200; // Lectura del sensor sumergido en agua (húmedo)

// Variables para almacenar lecturas de sensores
float tempAmbiente = 0.0;
float humAmbiente = 0.0;
int humSuelo = 0;

// ==========================================
// CONFIGURACIÓN DE RELÉS
// ==========================================
const int pinesRele[] = {13, 32, 27, 14};
const char* nombres[] = {"LUZ", "VENTILADOR", "BOMBA", "HUMIDIFICADOR"};

// Variables de estado del ciclo de relés
int releActual = 0;
bool releEncendido = false;

// ==========================================
// VARIABLES DE TIEMPO (TEMPORIZADORES NO BLOQUEANTES)
// ==========================================
unsigned long ultimoLecturaSensores = 0;
const unsigned long intervaloSensores = 1000; // Leer sensores cada 1 segundo

unsigned long ultimoCambioRele = 0;
const unsigned long tiempoEncendido = 3000;   // Tiempo encendido: 3 segundos
const unsigned long tiempoApagado = 1000;     // Tiempo de espera entre relés: 1 segundo
const unsigned long tiempoFinCiclo = 2000;    // Pausa al final del ciclo: 2 segundos

unsigned long ultimoCambioLCD = 0;
const unsigned long tiempoPaginaLCD = 3000;   // Alternar pantalla cada 3 segundos
int paginaLCD = 0;                            // 0 = Sensores, 1 = Relés

bool debeActualizarLCD = true;                // Bandera para actualizar el LCD de inmediato

void setup() {
  Serial.begin(115200);
  
  // Forzar pines I2C para ESP32 (SDA = 21, SCL = 22)
  Wire.begin(21, 22); 
  
  // Inicializar sensor DHT
  dht.begin();
  
  // Configurar y apagar todos los relés al inicio
  for (int i = 0; i < 4; i++) {
    pinMode(pinesRele[i], OUTPUT);
    digitalWrite(pinesRele[i], HIGH); // HIGH = Apagado en relés activos en bajo
  }
  
  // Inicializar Pantalla LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Germinadora IA");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando sist...");
  delay(2000);
  lcd.clear();
  
  // Iniciar primer ciclo de relé
  digitalWrite(pinesRele[releActual], LOW); // LOW = Encendido
  releEncendido = true;
  ultimoCambioRele = millis();
  Serial.print("Encendiendo: ");
  Serial.println(nombres[releActual]);
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
    
    // Validar lecturas del DHT11
    if (!isnan(t)) tempAmbiente = t;
    if (!isnan(h)) humAmbiente = h;
    
    // Leer sensor de humedad de suelo
    int lecturaSuelo = analogRead(PIN_SUELO);
    // Mapear el valor analógico a un porcentaje de 0% a 100%
    int pctSuelo = map(lecturaSuelo, VALOR_SECO, VALOR_HUMEDO, 0, 100);
    humSuelo = constrain(pctSuelo, 0, 100);
    
    // Mostrar en monitor serial para depuración
    Serial.print("Temp: "); Serial.print(tempAmbiente); Serial.print(" C | ");
    Serial.print("Hum. Aire: "); Serial.print(humAmbiente); Serial.print(" % | ");
    Serial.print("Lectura Suelo (ADC): "); Serial.print(lecturaSuelo); Serial.print(" | ");
    Serial.print("Hum. Suelo: "); Serial.print(humSuelo); Serial.println(" %");
    
    // Solicitar actualización del LCD porque los datos cambiaron
    debeActualizarLCD = true;
  }

  // =================================================================
  // 2. CONTROL DEL CICLO DE RELÉS (No Bloqueante)
  // =================================================================
  if (releActual < 4) {
    if (releEncendido) {
      // Si el relé está encendido, verificar si terminó su tiempo de encendido (3s)
      if (tiempoActual - ultimoCambioRele >= tiempoEncendido) {
        digitalWrite(pinesRele[releActual], HIGH); // APAGAR relé
        releEncendido = false;
        ultimoCambioRele = tiempoActual;
        
        Serial.print("Apagando: ");
        Serial.println(nombres[releActual]);
        
        debeActualizarLCD = true; // Actualizar LCD para reflejar el apagado
      }
    } else {
      // Si está apagado, verificar si terminó el tiempo de espera (1s) antes del siguiente relé
      if (tiempoActual - ultimoCambioRele >= tiempoApagado) {
        releActual++; // Pasar al siguiente relé
        
        if (releActual < 4) {
          digitalWrite(pinesRele[releActual], LOW); // ENCENDER relé
          releEncendido = true;
          ultimoCambioRele = tiempoActual;
          
          Serial.print("Encendiendo: ");
          Serial.println(nombres[releActual]);
        } else {
          // Fin del ciclo de los 4 relés
          ultimoCambioRele = tiempoActual;
          Serial.println("Ciclo completo. Esperando reinicio...");
        }
        
        debeActualizarLCD = true;
      }
    }
  } else {
    // Si terminó el ciclo, verificar si ya pasó el tiempo de espera de fin de ciclo (2s)
    if (tiempoActual - ultimoCambioRele >= tiempoFinCiclo) {
      releActual = 0; // Reiniciar al primer relé
      digitalWrite(pinesRele[releActual], LOW); // ENCENDER primer relé
      releEncendido = true;
      ultimoCambioRele = tiempoActual;
      
      Serial.print("Reiniciando ciclo. Encendiendo: ");
      Serial.println(nombres[releActual]);
      
      debeActualizarLCD = true;
    }
  }

  // =================================================================
  // 3. CAMBIO DE PÁGINAS DEL LCD (Cada 3 segundos)
  // =================================================================
  if (tiempoActual - ultimoCambioLCD >= tiempoPaginaLCD) {
    ultimoCambioLCD = tiempoActual;
    paginaLCD = (paginaLCD == 0) ? 1 : 0; // Alterna entre 0 y 1
    debeActualizarLCD = true;             // Forzar redibujo de la pantalla
  }

  // =================================================================
  // 4. ACTUALIZACIÓN FÍSICA DE LA PANTALLA LCD
  // =================================================================
  if (debeActualizarLCD) {
    actualizarLCD();
    debeActualizarLCD = false;
  }
}

// Función encargada de dibujar en el LCD según la página activa
void actualizarLCD() {
  lcd.clear(); // Limpia la pantalla para evitar que queden caracteres residuales
  
  if (paginaLCD == 0) {
    // ----------------------------------------------------
    // PÁGINA 0: LECTURAS DE SENSORES
    // ----------------------------------------------------
    // Fila 0: Temp y Humedad del Aire
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(tempAmbiente, 1); // Muestra 1 decimal (ej: 25.5)
    lcd.print("C H.A:");
    lcd.print((int)humAmbiente); // Muestra entero (ej: 60)
    lcd.print("%");
    
    // Fila 1: Humedad del Suelo
    lcd.setCursor(0, 1);
    lcd.print("H.Suelo: ");
    lcd.print(humSuelo);
    lcd.print("%");
    
  } else {
    // ----------------------------------------------------
    // PÁGINA 1: ESTADO DE ACTUADORES (RELÉS)
    // ----------------------------------------------------
    lcd.setCursor(0, 0);
    if (releActual < 4) {
      lcd.print("Activo: ");
      lcd.print(nombres[releActual]);
    } else {
      lcd.print("Ciclo completo ");
    }
    
    lcd.setCursor(0, 1);
    if (releActual < 4) {
      if (releEncendido) {
        lcd.print("Estado: ENCENDIDO");
      } else {
        lcd.print("Estado: APAGADO  ");
      }
    } else {
      lcd.print("Reiniciando...  ");
    }
  }
}
