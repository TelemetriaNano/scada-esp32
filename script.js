// Broker MQTT público con WebSocket
const broker = "wss://broker.hivemq.com:8884/mqtt";

// Tópicos (deben coincidir con el ESP32)
const topicTemp = "esp32/telemetria/temperatura";
const topicHum  = "esp32/telemetria/humedad";

const options = {
  clientId: "scada_web_" + Math.random().toString(16).substr(2, 8),
  clean: true,
  connectTimeout: 4000
};

const client = mqtt.connect(broker, options);

const statusEl = document.getElementById("status");
const tempEl   = document.getElementById("temp");
const humEl    = document.getElementById("hum");

client.on("connect", () => {
  console.log("Conectado a MQTT");
  statusEl.textContent = "Conectado";
  statusEl.className = "online";

  client.subscribe([topicTemp, topicHum]);
});

client.on("message", (topic, message) => {
  const value = message.toString();

  if (topic === topicTemp) {
    tempEl.textContent = value;
  }

  if (topic === topicHum) {
    humEl.textContent = value;
  }
});

client.on("close", () => {
  statusEl.textContent = "Desconectado";
  statusEl.className = "offline";
});