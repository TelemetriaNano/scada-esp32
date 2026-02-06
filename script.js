let lastEvent = "--";

async function updateStatus() {
  let data;
  try {
    // Intentamos conectarnos al ESP32
    const res = await fetch('/status');
    if (!res.ok) throw 'No response';
    data = await res.json();
  } catch {
    // Simulación para demo GitHub/WAN
    const pot = Math.floor(Math.random() * 101);
    const sw = Math.random() > 0.5;
    const led = Math.random() > 0.5;
    const alarm = pot > 78; // activa alarma según valor del pot
    data = { pot, sw, led, alarm };
  }

  document.getElementById('pot').innerText = data.pot + '%';
  const swEl = document.getElementById('sw');
  swEl.innerText = data.sw ? 'ON' : 'OFF';
  swEl.className = 'value btn ' + (data.sw ? 'on' : '');

  const alarmEl = document.getElementById('alarm');
  alarmEl.innerText = data.alarm ? 'ALARM' : 'OK';
  alarmEl.className = 'value btn ' + (data.alarm ? 'on' : '');

  // Evento simulado al presionar switch
  if(data.sw && lastEvent !== "Boton 001") {
    lastEvent = "Boton 001";
  } else if(!data.sw) {
    lastEvent = "--";
  }
  document.getElementById('event').innerText = lastEvent;
}

// Actualización cada segundo
setInterval(updateStatus, 1000);
updateStatus();