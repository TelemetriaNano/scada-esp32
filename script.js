const broker = "wss://broker.hivemq.com:8884/mqtt";

const client = mqtt.connect(broker, {
  clientId: "scada_web_" + Math.random().toString(16).substr(2,8)
});

client.on("connect", () => {
  client.subscribe([
    "esp32/telemetria/pot",
    "esp32/telemetria/switch",
    "esp32/alarmas/estado",
    "esp32/eventos/boton"
  ]);
});

client.on("message", (topic, msg) => {
  const v = msg.toString();

  if (topic.endsWith("/pot")) pot.innerText = v;
  if (topic.endsWith("/switch")) {
    sw.innerText = v === "1" ? "ON" : "OFF";
    sw.className = "badge " + (v === "1" ? "on":"off");
  }
  if (topic.endsWith("/estado")) {
    alarm.innerText = v === "1" ? "ALARM" : "OK";
    alarm.className = "badge " + (v === "1" ? "warn":"off");
    potCard.classList.toggle("warn", v === "1");
  }
  if (topic.endsWith("/boton")) event.innerText = v;
});