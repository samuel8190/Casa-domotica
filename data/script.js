// app.js
const apiInput = document.getElementById('apiBaseUrl');
const connectBtn = document.getElementById('connectBtn');
const tileTemp = document.getElementById('tile-temp');
const tileHum = document.getElementById('tile-hum');
const tileGas = document.getElementById('tile-gas');
const tileWater = document.getElementById('tile-water');
const tileMotion = document.getElementById('tile-motion');
const tileLed = document.getElementById('tile-led');
const ledIndicator = document.getElementById('ledIndicator');
const toggleLedBtn = document.getElementById('toggleLedBtn');

const openChartBtn = document.getElementById('openChartBtn');
const chartContainer = document.getElementById('chartContainer');
const sensorChartCanvas = document.getElementById('sensorChart');
const closeChart = document.getElementById('closeChart');
const downloadChart = document.getElementById('downloadChart');
const sensorSelect = document.getElementById('sensorSelect');

let fetchInterval = null;
let chartInstance = null;
const MAX_POINTS = 60;
const store = {
  temp: { label: 'Temperatura (°C)', data: [] },
  hum: { label: 'Humedad (%)', data: [] },
  gas: { label: 'Gas (ppm)', data: [] },
  water: { label: 'Agua (%)', data: [] },
  motion: { label: 'Movimiento', data: [] }
};

function getApiBase() {
  return apiInput.value.trim().replace(/\/$/, '');
}

async function fetchDataOnce() {
  const base = getApiBase();
  if (!base) return;
  try {
    const res = await fetch(`${base}/data`, { mode: "cors" });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const d = await res.json();
    updateUI(d);
    appendToStore(d);
    if (chartInstance) liveAppendToChart(d);
  } catch (err) {
    console.error('Fetch error', err);
  }
}

function updateUI(d) {
  tileTemp.textContent = d.temp !== undefined ? d.temp.toFixed(1) + ' °C' : '--';
  tileHum.textContent = d.hum !== undefined ? d.hum.toFixed(0) + ' %' : '--';
  tileGas.textContent = d.gas !== undefined ? d.gas + ' ppm' : '--';
  tileWater.textContent = d.water !== undefined ? d.water + ' %' : '--';
  tileMotion.textContent = d.motion ? 'DETECTADO' : 'Sin detección';
  updateLedDisplay(!!d.led);
}

function updateLedDisplay(state) {
  ledIndicator.classList.toggle('on', state);
  tileLed.textContent = state ? 'ENCENDIDO' : 'APAGADO';
  toggleLedBtn.textContent = state ? 'Apagar' : 'Encender';
  toggleLedBtn.dataset.state = state ? 'on' : 'off';
}

async function toggleLed() {
  const base = getApiBase();
  if (!base) return alert('Configura la URL del ESP32');
  const newState = toggleLedBtn.dataset.state === 'on' ? 'off' : 'on';
  try {
    toggleLedBtn.disabled = true;
    const res = await fetch(`${base}/led?state=${newState}`, { method: 'POST' });
    if (!res.ok) throw new Error('LED request failed');
    const r = await res.json();
    updateLedDisplay(r.led_state);
  } catch (err) {
    console.error(err);
    alert('Error controlando LED');
  } finally {
    toggleLedBtn.disabled = false;
  }
}

function appendToStore(d) {
  const now = new Date();
  function push(key, val) {
    store[key].data.push({ t: now, v: val });
    if (store[key].data.length > MAX_POINTS) store[key].data.shift();
  }
  push('temp', d.temp ?? null);
  push('hum', d.hum ?? null);
  push('gas', d.gas ?? null);
  push('water', d.water ?? null);
  push('motion', d.motion ? 1 : 0);
}

function startPolling() {
  if (fetchInterval) clearInterval(fetchInterval);
  fetchDataOnce();
  fetchInterval = setInterval(fetchDataOnce, 4000);
}

function stopPolling() {
  if (fetchInterval) clearInterval(fetchInterval);
  fetchInterval = null;
}

connectBtn.addEventListener('click', () => {
  localStorage.setItem('apiBaseUrl', apiInput.value.trim());
  startPolling();
});

toggleLedBtn.addEventListener('click', toggleLed);

window.onload = function() {
  const saved = localStorage.getItem('apiBaseUrl');
  if (saved) apiInput.value = saved;
  if (apiInput.value.trim()) startPolling();
};

// Chart handling
openChartBtn.addEventListener('click', () => {
  const key = sensorSelect.value;
  openChart(key);
});

function openChart(key) {
  if (chartInstance) chartInstance.destroy();
  chartContainer.style.display = 'block';
  const labels = store[key].data.map(s => new Date(s.t).toLocaleTimeString());
  const data = store[key].data.map(s => s.v);
  const cfg = {
    type: 'line',
    data: {
      labels,
      datasets: [{
        label: store[key].label,
        data,
        borderColor: '#2563eb',
        backgroundColor: 'rgba(37,99,235,0.08)',
        tension: 0.25,
        fill: true,
        pointRadius: 3
      }]
    },
    options: {
      responsive: true,
      animation: false,
      scales: { y: { beginAtZero: key === 'motion' ? true : false } }
    }
  };
  chartInstance = new Chart(sensorChartCanvas.getContext('2d'), cfg);
}

function liveAppendToChart(d) {
  if (!chartInstance) return;
  const key = sensorSelect.value;
  const now = new Date();
  if (chartInstance.data.labels.length >= MAX_POINTS) {
    chartInstance.data.labels.shift();
    chartInstance.data.datasets.forEach(ds => ds.data.shift());
  }
  chartInstance.data.labels.push(now.toLocaleTimeString());
  if (key === 'temp') chartInstance.data.datasets[0].data.push(d.temp);
  else if (key === 'hum') chartInstance.data.datasets[0].data.push(d.hum);
  else if (key === 'gas') chartInstance.data.datasets[0].data.push(d.gas);
  else if (key === 'water') chartInstance.data.datasets[0].data.push(d.water);
  else if (key === 'motion') chartInstance.data.datasets[0].data.push(d.motion ? 1 : 0);
  chartInstance.update('none');
}

closeChart.addEventListener('click', () => {
  if (chartInstance) chartInstance.destroy();
  chartInstance = null;
  chartContainer.style.display = 'none';
});

downloadChart.addEventListener('click', () => {
  if (!chartInstance) return;
  const url = chartInstance.toBase64Image();
  const a = document.createElement('a');
  a.href = url; a.download = 'grafica.png';
  document.body.appendChild(a); a.click(); a.remove();
});