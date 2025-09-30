// Variables globales
let sensorChart = null;
let currentSensor = 'temperature';

// Configuración de nombres de sensores
const sensorNames = {
    temperature: '🌡️ Temperatura',
    humidity: '💧 Humedad', 
    gas: '⚠️ Nivel de Gas',
    rain: '🌧️ Lluvia',
    motion: '🚶 Movimiento'
};

// Unidades de los sensores
const sensorUnits = {
    temperature: '°C',
    humidity: '%',
    gas: ' unidades',
    rain: ' unidades',
    motion: ''
};

// Inicializar la aplicación
document.addEventListener('DOMContentLoaded', function() {
    // Establecer fecha actual por defecto
    const today = new Date().toISOString().split('T')[0];
    document.getElementById('chartDate').value = today;
    document.getElementById('historyDate').value = today;
    
    // Cargar datos iniciales
    updateSensorData();
    loadChartData();
    
    // Actualizar datos cada 5 segundos
    setInterval(updateSensorData, 5000);
});

// Actualizar datos de sensores en tiempo real
async function updateSensorData() {
    try {
        const response = await fetch('/data');
        const data = await response.json();
        
        // Actualizar valores en las tarjetas
        document.getElementById('temperature').textContent = 
            data.temperature !== undefined ? `${data.temperature.toFixed(1)} °C` : '-- °C';
        
        document.getElementById('humidity').textContent = 
            data.humidity !== undefined ? `${data.humidity.toFixed(1)} %` : '-- %';
        
        document.getElementById('gas').textContent = 
            data.gasValue !== undefined ? data.gasValue : '--';
        
        document.getElementById('rain').textContent = 
            data.rainValue !== undefined ? data.rainValue : '--';
        
        document.getElementById('motion').textContent = 
            data.motionDetected !== undefined ? 
            (data.motionDetected ? '🟢 DETECTADO' : '⚪ NO') : '--';
            
    } catch (error) {
        console.error('Error al obtener datos:', error);
        // Datos de ejemplo para pruebas
        showDemoData();
    }
}

// Mostrar datos de demostración (para pruebas)
function showDemoData() {
    const now = new Date();
    
    document.getElementById('temperature').textContent = 
        `${(20 + Math.random() * 10).toFixed(1)} °C`;
    
    document.getElementById('humidity').textContent = 
        `${(40 + Math.random() * 30).toFixed(1)} %`;
    
    document.getElementById('gas').textContent = 
        Math.floor(100 + Math.random() * 500);
    
    document.getElementById('rain').textContent = 
        Math.floor(1000 + Math.random() * 2000);
    
    document.getElementById('motion').textContent = 
        Math.random() > 0.7 ? '🟢 DETECTADO' : '⚪ NO';
}

// Abrir modal de calendario
function openCalendar(sensorType) {
    currentSensor = sensorType;
    document.getElementById('modalSensorName').textContent = sensorNames[sensorType];
    document.getElementById('calendarModal').style.display = 'block';
}

// Cerrar modal de calendario
function closeCalendar() {
    document.getElementById('calendarModal').style.display = 'none';
}

// Cargar datos históricos
async function loadHistoricalData() {
    const date = document.getElementById('historyDate').value;
    
    if (!date) {
        alert('Por favor selecciona una fecha');
        return;
    }
    
    try {
        // Simular datos históricos (reemplazar con tu API real)
        const historicalData = generateDemoHistoricalData(date, currentSensor);
        displayHistoricalData(historicalData);
    } catch (error) {
        console.error('Error al cargar datos históricos:', error);
        // Mostrar datos de demostración
        const demoData = generateDemoHistoricalData(date, currentSensor);
        displayHistoricalData(demoData);
    }
}

// Generar datos de demostración para históricos
function generateDemoHistoricalData(date, sensorType) {
    const data = [];
    
    for (let hour = 0; hour < 24; hour++) {
        for (let minute = 0; minute < 60; minute += 30) { // Cada 30 minutos
            let value;
            
            switch(sensorType) {
                case 'temperature':
                    value = 18 + Math.random() * 12; // 18-30°C
                    break;
                case 'humidity':
                    value = 40 + Math.random() * 40; // 40-80%
                    break;
                case 'gas':
                    value = Math.floor(50 + Math.random() * 300); // 50-350
                    break;
                case 'rain':
                    value = Math.floor(Math.random() * 4096); // 0-4095
                    break;
                case 'motion':
                    value = Math.random() > 0.8 ? 1 : 0; // 20% probabilidad
                    break;
                default:
                    value = Math.random() * 100;
            }
            
            data.push({
                time: `${hour.toString().padStart(2, '0')}:${minute.toString().padStart(2, '0')}`,
                value: sensorType === 'temperature' || sensorType === 'humidity' ? 
                       Number(value.toFixed(1)) : Math.floor(value)
            });
        }
    }
    
    return data;
}

// Mostrar datos históricos en el modal
function displayHistoricalData(data) {
    const container = document.getElementById('historicalData');
    container.innerHTML = '';
    
    data.forEach(item => {
        const dataItem = document.createElement('div');
        dataItem.className = 'data-item';
        
        const valueDisplay = currentSensor === 'motion' ? 
            (item.value ? '🟢 DETECTADO' : '⚪ NO') : 
            `${item.value}${sensorUnits[currentSensor]}`;
            
        dataItem.innerHTML = `
            <div class="data-time">${item.time}</div>
            <div class="data-value">${valueDisplay}</div>
        `;
        
        container.appendChild(dataItem);
    });
}

// Cargar datos para el gráfico
async function loadChartData() {
    const sensorType = document.getElementById('sensorSelect').value;
    const date = document.getElementById('chartDate').value;
    
    if (!date) {
        alert('Por favor selecciona una fecha');
        return;
    }
    
    try {
        // Simular datos del gráfico (reemplazar con tu API real)
        const chartData = generateDemoChartData(date, sensorType);
        updateChart(chartData, sensorType);
    } catch (error) {
        console.error('Error al cargar datos del gráfico:', error);
        // Mostrar datos de demostración
        const demoData = generateDemoChartData(date, sensorType);
        updateChart(demoData, sensorType);
    }
}

// Generar datos de demostración para gráficos
function generateDemoChartData(date, sensorType) {
    const labels = [];
    const data = [];
    
    // Generar datos para cada hora del día
    for (let hour = 0; hour < 24; hour++) {
        labels.push(`${hour.toString().padStart(2, '0')}:00`);
        
        let value;
        switch(sensorType) {
            case 'temperature':
                // Simular variación diurna de temperatura
                value = 16 + 8 * Math.sin((hour - 6) * Math.PI / 12) + (Math.random() - 0.5) * 2;
                break;
            case 'humidity':
                value = 50 + 20 * Math.cos((hour - 12) * Math.PI / 12) + (Math.random() - 0.5) * 10;
                break;
            case 'gas':
                value = 100 + Math.random() * 200;
                break;
            case 'rain':
                value = Math.random() > 0.8 ? Math.random() * 2000 : 0; // Llueve 20% del tiempo
                break;
            default:
                value = Math.random() * 100;
        }
        
        data.push(sensorType === 'temperature' || sensorType === 'humidity' ? 
                 Number(value.toFixed(1)) : Math.floor(value));
    }
    
    return { labels, data };
}

// Actualizar el gráfico
function updateChart(chartData, sensorType) {
    const ctx = document.getElementById('sensorChart').getContext('2d');
    
    // Destruir gráfico anterior si existe
    if (sensorChart) {
        sensorChart.destroy();
    }
    
    // Configuración del gráfico
    const config = {
        type: 'line',
        data: {
            labels: chartData.labels,
            datasets: [{
                label: sensorNames[sensorType],
                data: chartData.data,
                borderColor: '#00b4db',
                backgroundColor: 'rgba(0, 180, 219, 0.1)',
                borderWidth: 3,
                fill: true,
                tension: 0.4,
                pointBackgroundColor: '#00b4db',
                pointBorderColor: '#ffffff',
                pointBorderWidth: 2,
                pointRadius: 4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    labels: {
                        color: '#e0e0e0',
                        font: {
                            size: 14
                        }
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    titleColor: '#00b4db',
                    bodyColor: '#e0e0e0'
                }
            },
            scales: {
                x: {
                    grid: {
                        color: 'rgba(255, 255, 255, 0.1)'
                    },
                    ticks: {
                        color: '#e0e0e0'
                    }
                },
                y: {
                    grid: {
                        color: 'rgba(255, 255, 255, 0.1)'
                    },
                    ticks: {
                        color: '#e0e0e0'
                    },
                    beginAtZero: sensorType !== 'temperature'
                }
            }
        }
    };
    
    sensorChart = new Chart(ctx, config);
}

// Cerrar modal al hacer clic fuera de él
window.onclick = function(event) {
    const modal = document.getElementById('calendarModal');
    if (event.target === modal) {
        closeCalendar();
    }
}