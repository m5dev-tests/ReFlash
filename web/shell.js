// ReFlash Web Shell Interfacing Logic

var Module = Module || {};

let reflashApi = {
    initEngine: null,
    loadFrame: null,
    setBlendMode: null,
    setColorTransform: null
};

let currentPattern = 0;
let frameCount = 0;
let lastTime = performance.now();
let fps = 0;

Module.onRuntimeInitialized = function() {
    console.log("ReFlash WebAssembly Engine Module Initialized");

    reflashApi.initEngine = Module.cwrap('init_engine', 'number', ['number', 'number']);
    reflashApi.loadFrame = Module.cwrap('load_frame', null, []);
    reflashApi.setBlendMode = Module.cwrap('set_blend_mode', null, ['number']);
    reflashApi.setColorTransform = Module.cwrap('set_color_transform', null, ['number', 'number', 'number', 'number', 'number', 'number', 'number', 'number']);

    const canvas = document.getElementById('reflash-canvas');
    if (reflashApi.initEngine(canvas.width, canvas.height)) {
        document.getElementById('stat-status').textContent = "Running (WebGL2)";
    } else {
        document.getElementById('stat-status').textContent = "Init Failed";
    }

    setupUI();
    requestAnimationFrame(renderLoop);
};

function setupUI() {
    const blendSelect = document.getElementById('blend-select');
    blendSelect.addEventListener('change', (e) => {
        const mode = parseInt(e.target.value, 10);
        if (reflashApi.setBlendMode) {
            reflashApi.setBlendMode(mode);
        }
    });

    const patternBtn = document.getElementById('pattern-btn');
    patternBtn.addEventListener('click', () => {
        currentPattern = (currentPattern + 1) % 3;
        console.log("Switched to test pattern: " + currentPattern);
    });
}

function updateStats() {
    frameCount++;
    const now = performance.now();
    if (now - lastTime >= 1000) {
        fps = Math.round((frameCount * 1000) / (now - lastTime));
        document.getElementById('stat-fps').textContent = fps;
        frameCount = 0;
        lastTime = now;

        if (Module.HEAP8) {
            const memoryMB = (Module.HEAP8.buffer.byteLength / (1024 * 1024)).toFixed(2);
            document.getElementById('stat-memory').textContent = memoryMB + " MB";
        }
    }
}

function renderLoop() {
    if (reflashApi.loadFrame) {
        reflashApi.loadFrame();
    }
    updateStats();
    requestAnimationFrame(renderLoop);
}
