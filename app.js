// =================================================================
// LÓGICA DE SIMULACIÓN EN TIEMPO REAL - GERMINADORA IA
// =================================================================

// Selectores de Elementos de la Interfaz (Deslizadores)
const tempSlider = document.getElementById('temp-slider');
const humAirSlider = document.getElementById('hum-air-slider');
const humSoilSlider = document.getElementById('hum-soil-slider');

// Selectores de Muestras de Valor Numérico
const tempVal = document.getElementById('temp-val');
const humAirVal = document.getElementById('hum-air-val');
const humSoilVal = document.getElementById('hum-soil-val');

// Botón de Luz
const lightToggleSim = document.getElementById('light-toggle-sim');

// Variables de Estado de la Simulación (Iguales al ESP32)
let temp = 22.0;
let humAir = 70;
let humSoil = 65;

let luzEstado = true;
let ventiladorEstado = false;
let humidificadorEstado = false;

let bombaRegando = false;
let bombaBloqueada = false;
let lockoutRemaining = 0; // Segundos restantes de bloqueo en simulación

// =================================================================
// LOGICA DE RIEGO (BOMBA)
// =================================================================
function iniciarRiego() {
    bombaRegando = true;
    let tiempoRiegoRestante = 3; // Simula el riego de 3 segundos
    
    const pumpStatus = document.getElementById('pump-status');
    const actBomba = document.getElementById('act-bomba');
    
    actBomba.classList.add('active');
    pumpStatus.textContent = `Regando... (${tiempoRiegoRestante}s)`;
    
    const intervaloRiego = setInterval(() => {
        tiempoRiegoRestante--;
        if (tiempoRiegoRestante > 0) {
            pumpStatus.textContent = `Regando... (${tiempoRiegoRestante}s)`;
        } else {
            clearInterval(intervaloRiego);
            bombaRegando = false;
            bombaBloqueada = true;
            lockoutRemaining = 10; // Bloqueo simulado de 10s para pruebas rápidas
            
            // Iniciar cuenta regresiva del bloqueo
            const intervaloLockout = setInterval(() => {
                lockoutRemaining--;
                if (lockoutRemaining > 0) {
                    actualizarSistema();
                } else {
                    clearInterval(intervaloLockout);
                    bombaBloqueada = false;
                    actualizarSistema();
                }
            }, 1000);
            
            actualizarSistema();
        }
    }, 1000);
}

// =================================================================
// FUNCIÓN PRINCIPAL DE DECISIONES DE ACTUADORES
// =================================================================
function actualizarSistema() {
    // 1. Control del Fotoperíodo (LUZ)
    const actLuz = document.getElementById('act-luz');
    const luzStatus = document.getElementById('luz-status');
    
    if (luzEstado) {
        actLuz.classList.add('active');
        luzStatus.textContent = "Activa (Día: 14h)";
    } else {
        actLuz.classList.remove('active');
        luzStatus.textContent = "Apagada (Noche: 10h)";
    }

    // 2. Control del Ventilador (Temperatura / Humedad del aire alta)
    // Con histéresis igual que en C++
    const actVent = document.getElementById('act-ventilador');
    const ventStatus = document.getElementById('vent-status');
    
    if (!ventiladorEstado) {
        // Condición para encender
        if (temp > 25.0 || humAir > 80) {
            ventiladorEstado = true;
        }
    } else {
        // Condición para apagar
        if (temp < 24.0 && humAir < 75) {
            ventiladorEstado = false;
        }
    }
    
    if (ventiladorEstado) {
        actVent.classList.add('active');
        if (temp > 25.0 && humAir > 80) {
            ventStatus.textContent = "Encendido (Calor + Humedad alta)";
        } else if (temp > 25.0) {
            ventStatus.textContent = "Encendido (Alta Temp > 25°C)";
        } else {
            ventStatus.textContent = "Encendido (H.A. Alta > 80%)";
        }
    } else {
        actVent.classList.remove('active');
        ventStatus.textContent = "Apagado (Rango Seguro)";
    }

    // 3. Control del Humidificador (Humedad del aire baja)
    const actHumi = document.getElementById('act-humidificador');
    const humiStatus = document.getElementById('humi-status');
    
    if (!humidificadorEstado) {
        if (humAir < 65) {
            humidificadorEstado = true;
        }
    } else {
        if (humAir > 75) {
            humidificadorEstado = false;
        }
    }
    
    if (humidificadorEstado) {
        actHumi.classList.add('active');
        humiStatus.textContent = "Encendido (H.A. Baja < 65%)";
    } else {
        actHumi.classList.remove('active');
        humiStatus.textContent = "Apagado (Rango Seguro)";
    }

    // 4. Control de la Bomba de Agua (Humedad de suelo baja)
    const actBomba = document.getElementById('act-bomba');
    const pumpStatus = document.getElementById('pump-status');
    
    if (bombaRegando) {
        actBomba.classList.add('active');
        actBomba.classList.remove('lockout');
        // El texto se actualiza dentro del intervalo de riego
    } else if (bombaBloqueada) {
        actBomba.classList.remove('active');
        actBomba.classList.add('lockout');
        pumpStatus.textContent = `Bloqueo de Seguridad (${lockoutRemaining}s)`;
    } else {
        actBomba.classList.remove('active');
        actBomba.classList.remove('lockout');
        pumpStatus.textContent = "Inactiva (H. Suelo OK)";
        
        // Disparador automático
        if (humSoil < 60) {
            iniciarRiego();
        }
    }
}

// =================================================================
// EVENT LISTENERS (INTERACCIONES DEL USUARIO)
// =================================================================

// Escuchar cambios en los deslizadores
tempSlider.addEventListener('input', (e) => {
    temp = parseFloat(e.target.value);
    tempVal.textContent = temp.toFixed(1);
    actualizarSistema();
});

humAirSlider.addEventListener('input', (e) => {
    humAir = parseInt(e.target.value);
    humAirVal.textContent = humAir;
    actualizarSistema();
});

humSoilSlider.addEventListener('input', (e) => {
    humSoil = parseInt(e.target.value);
    humSoilVal.textContent = humSoil;
    actualizarSistema();
});

// Escuchar click en el botón de simulación de luz
lightToggleSim.addEventListener('click', () => {
    luzEstado = !luzEstado;
    actualizarSistema();
});

// Inicialización de la Interfaz
actualizarSistema();
console.log("Simulador de Germinadora IA inicializado correctamente.");
