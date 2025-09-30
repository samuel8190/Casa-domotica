let chart;
let sensorSeleccionado = null;

// === ACTUALIZACIÓN EN TIEMPO REAL ===
async function actualizarDatos() {
  try {
    const res = await fetch('/data');
    const data = await res.json();

    document.getElementById('temp').innerText = data.temperature + " °C";
    document.getElementById('hum').innerText = data.humidity + " %";
    document.getElementById('gas').innerText = data.gasValue;
    document.getElementById('rain').innerText = data.rainValue;
    document.getElementById('motion').innerText = data.motionDetected ? "Sí" : "No";
  } catch (err) {
    console.error("Error obteniendo datos:", err);
  }
}
setInterval(actualizarDatos, 2000);
actualizarDatos();

// === MOSTRAR CALENDARIO Y GRAFICA ===
function mostrarCalendario(sensor) {
  sensorSeleccionado = sensor;
  document.getElementById("grafica").classList.remove("hidden");
  document.getElementById("grafica-titulo").innerText = "Gráfica de " + sensor;
}

function cerrarGrafica() {
  document.getElementById("grafica").classList.add("hidden");
}

// === CARGAR GRAFICA ===
async function cargarGrafica() {
  if (!sensorSeleccionado) return;

  const fecha = document.getElementById("fecha").value;
  try {
    const res = await fetch(`/graph?sensor=${sensorSeleccionado}&date=${fecha}`);
    const data = await res.json();

    const labels = data.map(d => d.time);
    const valores = data.map(d => d.value);

    if (chart) chart.destroy();
    const ctx = document.getElementById('chart').getContext('2d');
    chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: labels,
        datasets: [{
          label: sensorSeleccionado,
          data: valores,
          borderColor: 'cyan',
          backgroundColor: 'rgba(0,255,255,0.2)',
          tension: 0.1
        }]
      },
      options: {
        scales: {
          x: { ticks: { color: '#ccc' } },
          y: { ticks: { color: '#ccc' } }
        },
        plugins: {
          legend: { labels: { color: '#fff' } }
        }
      }
    });
  } catch (err) {
    console.error("Error cargando grafica:", err);
  }
}

// === POPUP DE GASES ===
function mostrarGases() {
  document.getElementById("popup-gas").classList.remove("hidden");
}
function cerrarPopupGas() {
  document.getElementById("popup-gas").classList.add("hidden");
}
