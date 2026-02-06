async function updateStatus() {
  try {
    const response = await fetch('/status');
    if (!response.ok) return;

    const data = await response.json();

    const pot = document.getElementById('pot');
    const sw = document.getElementById('sw');
    const led = document.getElementById('led');
    const potCard = document.getElementById('potCard');

    pot.innerText = data.pot;
    sw.innerText = data.sw ? 'ON' : 'OFF';
    sw.className = 'badge ' + (data.sw ? 'on' : 'off');

    led.innerText = data.led ? 'ON' : 'OFF';
    led.className = 'badge ' + (data.led ? 'on' : 'off');

    if (data.alarm) {
      potCard.classList.add('warn');
    } else {
      potCard.classList.remove('warn');
    }
  } catch (err) {
    console.error('Error al actualizar status', err);
  }
}

document.getElementById('toggleBtn').addEventListener('click', async () => {
  try {
    await fetch('/toggle', { method: 'POST' });
  } catch (err) {
    console.error('Error al togglear LED', err);
  }
});

// Actualización periódica cada 1 segundo
setInterval(updateStatus, 1000);
updateStatus();