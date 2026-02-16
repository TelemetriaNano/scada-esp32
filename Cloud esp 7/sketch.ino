/*
 * SCADA NANO v2.0 - EXPANDIDO
 * 
 * Hardware:
 * - 2 Potenciómetros (nivel, presión)
 * - 1 Servo motor (válvula)
 * - 2 LEDs controlables (bombas)
 * - 1 LED alarma
 * - 2 Switches (sensores puerta/nivel)
 * 
 * Autor: SCADA NANO Team
 * Fecha: 2025
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// ===== CONFIGURACIÓN =====
#define MQTT_BROKER "430b579b966d4e73a269ce0c803564a7.s1.eu.hivemq.cloud"
#define MQTT_PORT   8883
#define MQTT_USER   "Telemetria-Nano"
#define MQTT_PASS   "Telemetria01"
#define MQTT_CLIENT "ESP32_SCADA_NANO_V2_EXPANDIDO"

// PINES - ENTRADAS ANALÓGICAS
#define PIN_POT1         34  // Potenciómetro 1 - Nivel
#define PIN_POT2         35  // Potenciómetro 2 - Presión

// PINES - SALIDAS DIGITALES
#define PIN_LED_BOMBA1   25  // LED Verde - Bomba 1
#define PIN_LED_BOMBA2   26  // LED Azul - Bomba 2
#define PIN_LED_ALARM    27  // LED Rojo - Alarma
#define PIN_SERVO        18  // Servo - Válvula

// PINES - ENTRADAS DIGITALES
#define PIN_SWITCH1      32  // Switch 1 - Sensor Puerta
#define PIN_SWITCH2      33  // Switch 2 - Sensor Nivel

// UMBRALES
#define ALARM_ON         80
#define ALARM_OFF        76

// ← OPTIMIZADO: Intervalos más largos = Menor uso de CPU
#define TELEMETRY_INTERVAL_MS  3000  // Publicar cada 3 segundos (era 1s)
#define MQTT_RECONNECT_DELAY_MS 5000
#define WIFI_RECONNECT_DELAY_MS 10000
#define WATCHDOG_TIMEOUT_S      30

// ===== CLASES DE UTILIDAD =====

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
private:
    static LogLevel level;
    
    static const char* levelStr(LogLevel l) {
        switch(l) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            default: return "?????";
        }
    }
    
    static void log(LogLevel l, const String& msg) {
        if (l < level) return;
        Serial.printf("[%10lu] [%s] %s\n", millis(), levelStr(l), msg.c_str());
    }
    
public:
    static void setLevel(LogLevel l) { level = l; }
    static void debug(const String& msg) { log(LogLevel::DEBUG, msg); }
    static void info(const String& msg)  { log(LogLevel::INFO, msg); }
    static void warn(const String& msg)  { log(LogLevel::WARN, msg); }
    static void error(const String& msg) { log(LogLevel::ERROR, msg); }
};

LogLevel Logger::level = LogLevel::INFO;

class Watchdog {
private:
    static unsigned long lastFeed;
    static uint32_t timeoutMs;
    static bool enabled;
    
public:
    static void begin(uint32_t timeoutSec = 30) {
        timeoutMs = timeoutSec * 1000;
        lastFeed = millis();
        enabled = true;
        Logger::info("✓ Watchdog activado: " + String(timeoutSec) + "s");
    }
    
    static void feed() {
        lastFeed = millis();
    }
    
    static void check() {
        if (!enabled) return;
        if (millis() - lastFeed > timeoutMs) {
            Logger::error("⚠️ WATCHDOG TIMEOUT - REINICIANDO...");
            delay(100);
            ESP.restart();
        }
    }
};

unsigned long Watchdog::lastFeed = 0;
uint32_t Watchdog::timeoutMs = 30000;
bool Watchdog::enabled = false;

// ===== GESTORES =====

class WiFiManager {
private:
    unsigned long lastAttempt = 0;
    bool connected = false;
    
public:
    void begin() {
        WiFi.mode(WIFI_STA);
        Logger::info("Iniciando WiFi...");
        WiFi.begin("Wokwi-GUEST", "");
    }
    
    bool update() {
        if (WiFi.status() == WL_CONNECTED) {
            if (!connected) {
                Logger::info("✓ WiFi conectado - IP: " + WiFi.localIP().toString());
                connected = true;
            }
            return true;
        }
        
        connected = false;
        
        if (millis() - lastAttempt > WIFI_RECONNECT_DELAY_MS) {
            Logger::warn("WiFi desconectado. Reintentando...");
            WiFi.reconnect();
            lastAttempt = millis();
        }
        
        return false;
    }
    
    bool isConnected() const { return connected; }
};

class MQTTManager {
private:
    WiFiClientSecure espClient;
    PubSubClient mqtt;
    unsigned long lastReconnect = 0;
    bool connected = false;
    
    static MQTTManager* instance;
    
    // ← NUEVO: Variables para rastrear estado de actuadores
    bool bomba1CurrentState = false;
    bool bomba2CurrentState = false;
    int servoCurrentAngle = 0;
    
    static void callback(char* topic, byte* payload, unsigned int length) {
        String msg = "";
        for (unsigned int i = 0; i < length; i++) {
            msg += (char)payload[i];
        }
        
        Logger::info("MQTT RX [" + String(topic) + "]: " + msg);
        
        // Delegar al handler de la instancia
        if (instance) {
            instance->handleMessage(topic, msg);
        }
    }
    
    void handleMessage(const String& topic, const String& msg) {
        // Control LED Bomba 1
        if (topic == "esp32/control/bomba1") {
            bomba1CurrentState = (msg == "1");
            digitalWrite(PIN_LED_BOMBA1, bomba1CurrentState ? HIGH : LOW);
            Logger::info("Bomba 1: " + msg);
            
            // ← NUEVO: Publicar estado confirmado
            publishState("bomba1", bomba1CurrentState);
        }
        // Control LED Bomba 2
        else if (topic == "esp32/control/bomba2") {
            bomba2CurrentState = (msg == "1");
            digitalWrite(PIN_LED_BOMBA2, bomba2CurrentState ? HIGH : LOW);
            Logger::info("Bomba 2: " + msg);
            
            // ← NUEVO: Publicar estado confirmado
            publishState("bomba2", bomba2CurrentState);
        }
        // Control Servo (válvula)
        else if (topic == "esp32/control/valvula") {
            servoCurrentAngle = msg.toInt();
            servoCurrentAngle = constrain(servoCurrentAngle, 0, 180);
            extern Servo servoValvula;
            servoValvula.write(servoCurrentAngle);
            Logger::info("Válvula: " + String(servoCurrentAngle) + "°");
            
            // ← NUEVO: Publicar estado confirmado
            publishState("valvula", servoCurrentAngle);
        }
        // ← NUEVO: Solicitud de estado actual
        else if (topic == "esp32/control/request_state") {
            Logger::info("📤 Enviando estado actual de todos los actuadores...");
            publishAllStates();
        }
    }
    
    // ← NUEVO: Publicar estado de un actuador específico
    void publishState(const String& actuator, bool state) {
        String topic = "esp32/estado/" + actuator;
        mqtt.publish(topic.c_str(), state ? "1" : "0", true); // retained = true
    }
    
    void publishState(const String& actuator, int value) {
        String topic = "esp32/estado/" + actuator;
        mqtt.publish(topic.c_str(), String(value).c_str(), true); // retained = true
    }
    
    // ← NUEVO: Publicar estado de todos los actuadores
    void publishAllStates() {
        publishState("bomba1", bomba1CurrentState);
        publishState("bomba2", bomba2CurrentState);
        publishState("valvula", servoCurrentAngle);
        Logger::info("✓ Estados publicados: Bomba1=" + String(bomba1CurrentState) + 
                    " Bomba2=" + String(bomba2CurrentState) + 
                    " Válvula=" + String(servoCurrentAngle) + "°");
    }
    
public:
    MQTTManager() : mqtt(espClient) {
        instance = this;
    }
    
    void begin() {
        espClient.setInsecure(); // ⚠️ SOLO PARA DESARROLLO
        mqtt.setServer(MQTT_BROKER, MQTT_PORT);
        mqtt.setCallback(callback);
        mqtt.setKeepAlive(60);
        Logger::info("MQTT configurado");
    }
    
    bool connect() {
        if (mqtt.connected()) return true;
        
        if (millis() - lastReconnect < MQTT_RECONNECT_DELAY_MS) {
            return false;
        }
        
        Logger::info("Intentando MQTT...");
        
        if (mqtt.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
            Logger::info("✓ MQTT conectado");
            connected = true;
            
            // Suscribirse a tópicos de control
            mqtt.subscribe("esp32/control/bomba1");
            mqtt.subscribe("esp32/control/bomba2");
            mqtt.subscribe("esp32/control/valvula");
            mqtt.subscribe("esp32/control/request_state"); // ← NUEVO
            
            mqtt.publish("esp32/system/status", "online", true);
            
            // ← NUEVO: Publicar estado inicial de todos los actuadores
            publishAllStates();
            
            return true;
        }
        
        Logger::error("MQTT falló. Código: " + String(mqtt.state()));
        lastReconnect = millis();
        return false;
    }
    
    void update() {
        if (mqtt.connected()) {
            mqtt.loop();
        } else {
            connected = false;
            connect();
        }
    }
    
    bool publish(const char* topic, const char* payload, bool retain = false) {
        if (!mqtt.connected()) return false;
        return mqtt.publish(topic, payload, retain);
    }
    
    bool isConnected() const { return connected; }
};

MQTTManager* MQTTManager::instance = nullptr;

class SensorManager {
private:
    // Valores de sensores
    int pot1Value = 0;
    int pot1Percent = 0;
    int pot2Value = 0;
    int pot2Percent = 0;
    bool switch1State = false;
    bool switch2State = false;
    
    // Estado de alarma
    bool alarmActive = false;
    int alarmCount = 0;
    
    unsigned long lastTelemetry = 0;
    unsigned long lastStatePublish = 0;  // ← NUEVO: Para publicar estados periódicamente
    
public:
    void begin() {
        // Configurar pines digitales de entrada
        pinMode(PIN_SWITCH1, INPUT);
        pinMode(PIN_SWITCH2, INPUT);
        
        // Configurar pines de salida
        pinMode(PIN_LED_BOMBA1, OUTPUT);
        pinMode(PIN_LED_BOMBA2, OUTPUT);
        pinMode(PIN_LED_ALARM, OUTPUT);
        
        digitalWrite(PIN_LED_BOMBA1, LOW);
        digitalWrite(PIN_LED_BOMBA2, LOW);
        digitalWrite(PIN_LED_ALARM, LOW);
        
        Logger::info("✓ Sensores y actuadores inicializados");
    }
    
    void readSensors() {
        // Leer potenciómetros
        pot1Value = analogRead(PIN_POT1);
        pot1Percent = map(pot1Value, 0, 4095, 0, 100);
        
        pot2Value = analogRead(PIN_POT2);
        pot2Percent = map(pot2Value, 0, 4095, 0, 100);
        
        // Leer switches (activo alto)
        switch1State = (digitalRead(PIN_SWITCH1) == HIGH);
        switch2State = (digitalRead(PIN_SWITCH2) == HIGH);
    }
    
    void processAlarms() {
        // Alarma si POT1 (nivel) está muy alto
        if (pot1Percent >= ALARM_ON && !alarmActive) {
            alarmActive = true;
            alarmCount++;
            digitalWrite(PIN_LED_ALARM, HIGH);
            Logger::warn("🚨 ALARMA! Nivel crítico: " + String(pot1Percent) + "%");
        } 
        else if (pot1Percent <= ALARM_OFF && alarmActive) {
            alarmActive = false;
            digitalWrite(PIN_LED_ALARM, LOW);
            Logger::info("✓ Nivel normalizado");
        }
    }
    
    void publishTelemetry(MQTTManager& mqtt) {
        if (millis() - lastTelemetry < TELEMETRY_INTERVAL_MS) {
            return;
        }
        
        Logger::debug("TX -> Pot1:" + String(pot1Percent) + "% Pot2:" + String(pot2Percent) + 
                     "% Sw1:" + String(switch1State) + " Sw2:" + String(switch2State));
        
        // Publicar sensores analógicos
        mqtt.publish("esp32/sensores/nivel", String(pot1Percent).c_str());
        mqtt.publish("esp32/sensores/presion", String(pot2Percent).c_str());
        
        // Publicar sensores digitales
        mqtt.publish("esp32/sensores/puerta", switch1State ? "1" : "0");
        mqtt.publish("esp32/sensores/sensor_nivel", switch2State ? "1" : "0");
        
        // Publicar estado de alarma
        mqtt.publish("esp32/alarmas/estado", alarmActive ? "1" : "0");
        mqtt.publish("esp32/alarmas/conteo", String(alarmCount).c_str());
        
        // Estadísticas del sistema
        mqtt.publish("esp32/system/memory", String(ESP.getFreeHeap()).c_str());
        mqtt.publish("esp32/system/uptime", String(millis() / 1000).c_str());
        
        lastTelemetry = millis();
    }
    
    // ← NUEVO: Publicar estado REAL de actuadores periódicamente
    void publishActuatorStates(MQTTManager& mqtt) {
        // ← OPTIMIZADO: Cada 5 segundos (era 2s) para reducir CPU
        if (millis() - lastStatePublish < 5000) return;
        
        // Leer el estado REAL de los pines (no memoria, sino hardware)
        bool bomba1Real = digitalRead(PIN_LED_BOMBA1);
        bool bomba2Real = digitalRead(PIN_LED_BOMBA2);
        
        // Publicar con retained=true para que nuevos dashboards lo reciban
        mqtt.publish("esp32/estado/bomba1", bomba1Real ? "1" : "0", true);
        mqtt.publish("esp32/estado/bomba2", bomba2Real ? "1" : "0", true);
        
        Logger::debug("Estado actual → Bomba1:" + String(bomba1Real) + " Bomba2:" + String(bomba2Real));
        
        lastStatePublish = millis();
    }
    
    void update(MQTTManager& mqtt) {
        readSensors();
        processAlarms();
        
        if (mqtt.isConnected()) {
            publishTelemetry(mqtt);
            publishActuatorStates(mqtt);  // ← NUEVO: Publicar estados cada 2s
        }
    }
};

// ===== INSTANCIAS GLOBALES =====
WiFiManager wifiMgr;
MQTTManager mqttMgr;
SensorManager sensorMgr;
Servo servoValvula;

// ===== SETUP =====
void setup() {
    Serial.begin(115200);
    delay(100);
    
    Logger::setLevel(LogLevel::INFO);
    
    Serial.println("\n╔═══════════════════════════════════════════════╗");
    Serial.println("║  SCADA NANO v2.0 EXPANDIDO - INICIANDO      ║");
    Serial.println("║  2 Pots | 1 Servo | 2 LEDs | 2 Switches     ║");
    Serial.println("╚═══════════════════════════════════════════════╝\n");
    
    // Inicializar servo
    servoValvula.attach(PIN_SERVO);
    servoValvula.write(0); // Posición inicial
    Logger::info("✓ Servo inicializado en posición 0°");
    
    // Inicializar watchdog
    Watchdog::begin(WATCHDOG_TIMEOUT_S);
    
    // Inicializar componentes
    wifiMgr.begin();
    mqttMgr.begin();
    sensorMgr.begin();
    
    Logger::info("✓ Sistema inicializado correctamente");
    Logger::info("Memoria libre: " + String(ESP.getFreeHeap()) + " bytes");
}

// ===== LOOP =====
void loop() {
    Watchdog::feed();
    Watchdog::check();
    
    if (wifiMgr.update()) {
        mqttMgr.update();
        
        if (mqttMgr.isConnected()) {
            sensorMgr.update(mqttMgr);
        }
    }
    
    delay(10);
}
