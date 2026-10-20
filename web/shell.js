// ReFlash Web Shell Interfacing Logic

var Module = Module || {};

let reflashApi = {
    initEngine: null,
    loadFrame: null,
    setBlendMode: null,
    setColorTransform: null,
    loadSwf: null,
    getWidth: null,
    getHeight: null,
    getFramerate: null,
    getFrameCount: null,
    getVersion: null,
    getAsVersion: null,
    getBgColor: null
};

let frameCount = 0;
let lastTime = performance.now();
let fps = 0;

Module.onRuntimeInitialized = function() {
    console.log("ReFlash WebAssembly Engine Module Initialized");

    reflashApi.initEngine = Module.cwrap('init_engine', 'number', ['number', 'number']);
    reflashApi.loadFrame = Module.cwrap('load_frame', null, []);
    reflashApi.setBlendMode = Module.cwrap('set_blend_mode', null, ['number']);
    reflashApi.setColorTransform = Module.cwrap('set_color_transform', null, ['number', 'number', 'number', 'number', 'number', 'number', 'number', 'number']);

    reflashApi.loadSwf = Module.cwrap('reflash_load_swf', 'number', ['number', 'number']);
    reflashApi.getWidth = Module.cwrap('reflash_get_width', 'number', []);
    reflashApi.getHeight = Module.cwrap('reflash_get_height', 'number', []);
    reflashApi.getFramerate = Module.cwrap('reflash_get_framerate', 'number', []);
    reflashApi.getFrameCount = Module.cwrap('reflash_get_frame_count', 'number', []);
    reflashApi.getVersion = Module.cwrap('reflash_get_version', 'number', []);
    reflashApi.getAsVersion = Module.cwrap('reflash_get_as_version', 'number', []);
    reflashApi.getBgColor = Module.cwrap('reflash_get_bg_color', 'number', []);

    const canvas = document.getElementById('reflash-canvas');
    if (reflashApi.initEngine(canvas.width, canvas.height)) {
        document.getElementById('stat-status').textContent = "Running (WebGL2)";
    } else {
        document.getElementById('stat-status').textContent = "Init Failed";
    }

    setupUI();
    setupDragAndDrop();
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

    const filePicker = document.getElementById('swf-file-picker');
    filePicker.addEventListener('change', (e) => {
        if (e.target.files && e.target.files.length > 0) {
            processInputFile(e.target.files[0]);
        }
    });
}

function setupDragAndDrop() {
    const dropTarget = document.getElementById('drop-target');
    const canvasWrapper = document.getElementById('canvas-wrapper');

    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
        dropTarget.addEventListener(eventName, (e) => {
            e.preventDefault();
            e.stopPropagation();
        }, false);
    });

    ['dragenter', 'dragover'].forEach(eventName => {
        dropTarget.addEventListener(eventName, () => {
            canvasWrapper.classList.add('drag-over');
        }, false);
    });

    ['dragleave', 'drop'].forEach(eventName => {
        dropTarget.addEventListener(eventName, () => {
            canvasWrapper.classList.remove('drag-over');
        }, false);
    });

    dropTarget.addEventListener('drop', (e) => {
        const dt = e.dataTransfer;
        const files = dt.files;
        if (files && files.length > 0) {
            processInputFile(files[0]);
        }
    }, false);
}

function processInputFile(file) {
    const nameLower = file.name.toLowerCase();
    if (nameLower.endsWith('.zip')) {
        processZipArchive(file);
    } else if (nameLower.endsWith('.swf')) {
        processSwfFile(file);
    } else {
        alert("Please select or drop a valid .swf file or .zip archive!");
    }
}

function processZipArchive(file) {
    if (typeof JSZip === 'undefined') {
        alert("JSZip library is not loaded.");
        return;
    }

    const reader = new FileReader();
    reader.onload = async function(e) {
        try {
            const zip = await JSZip.loadAsync(e.target.result);
            let mainSwfData = null;
            let mainSwfName = "";

            for (const filename of Object.keys(zip.files)) {
                const zipEntry = zip.files[filename];
                if (zipEntry.dir) continue;

                const fileData = await zipEntry.async("uint8array");
                // Unpack into Emscripten virtual filesystem root
                if (Module.FS && Module.FS.writeFile) {
                    const cleanPath = filename.startsWith('/') ? filename : '/' + filename;
                    const parts = cleanPath.split('/');
                    if (parts.length > 2) {
                        let curDir = "";
                        for (let i = 1; i < parts.length - 1; i++) {
                            curDir += "/" + parts[i];
                            try { Module.FS.mkdir(curDir); } catch(err) {}
                        }
                    }
                    Module.FS.writeFile(cleanPath, fileData);
                }

                const baseName = filename.split('/').pop().toLowerCase();
                if (baseName.endsWith('.swf')) {
                    if (!mainSwfData || baseName === 'main.swf') {
                        mainSwfData = fileData;
                        mainSwfName = filename;
                    }
                }
            }

            if (mainSwfData) {
                const ptr = Module._malloc(mainSwfData.length);
                Module.HEAPU8.set(mainSwfData, ptr);
                const success = reflashApi.loadSwf(ptr, mainSwfData.length);
                Module._free(ptr);

                if (success === 1) {
                    updateHUD({ name: mainSwfName, size: mainSwfData.length }, mainSwfData);
                } else {
                    alert("Failed to parse root SWF binary header from ZIP.");
                }
            } else {
                alert("No .swf file found in the ZIP archive.");
            }
        } catch (err) {
            console.error("Failed to extract ZIP archive:", err);
            alert("Error reading ZIP archive.");
        }
    };
    reader.readAsArrayBuffer(file);
}

function processSwfFile(file) {
    const reader = new FileReader();
    reader.onload = function(e) {
        const arrayBuffer = e.target.result;
        const byteArray = new Uint8Array(arrayBuffer);

        // Write file to Emscripten virtual filesystem root
        if (Module.FS && Module.FS.writeFile) {
            try {
                Module.FS.writeFile('/' + file.name, byteArray);
            } catch (err) {
                console.warn("VFS write warning:", err);
            }
        }

        // Allocate memory in Wasm heap
        const ptr = Module._malloc(byteArray.length);
        Module.HEAPU8.set(byteArray, ptr);

        // Call reflash_load_swf
        const success = reflashApi.loadSwf(ptr, byteArray.length);
        Module._free(ptr);

        if (success === 1) {
            updateHUD(file, byteArray);
        } else {
            alert("Failed to parse SWF binary header or payload.");
        }
    };
    reader.readAsArrayBuffer(file);
}

function updateHUD(file, byteArray) {
    const width = Math.round(reflashApi.getWidth());
    const height = Math.round(reflashApi.getHeight());
    const fpsVal = reflashApi.getFramerate().toFixed(1);
    const frameCountVal = reflashApi.getFrameCount();
    const versionVal = reflashApi.getVersion();
    const asVersionVal = reflashApi.getAsVersion();
    const bgColorVal = reflashApi.getBgColor();

    const sigChar = String.fromCharCode(byteArray[0], byteArray[1], byteArray[2]);

    // Resize canvas
    const canvas = document.getElementById('reflash-canvas');
    canvas.width = width;
    canvas.height = height;

    if (reflashApi.initEngine) {
        reflashApi.initEngine(width, height);
    }

    // Convert bg color to hex string
    const hexColor = "#" + ("000000" + bgColorVal.toString(16)).slice(-6).toUpperCase();
    canvas.style.backgroundColor = hexColor;

    // Format file size
    let sizeStr = (file.size / 1024).toFixed(1) + " KB";
    if (file.size >= 1024 * 1024) {
        sizeStr = (file.size / (1024 * 1024)).toFixed(2) + " MB";
    }

    document.getElementById('hud-file').textContent = file.name;
    document.getElementById('hud-size').textContent = sizeStr;
    document.getElementById('hud-signature').textContent = sigChar + " (" + (sigChar === 'CWS' ? "Zlib" : "Uncompressed") + ")";
    document.getElementById('hud-version').textContent = "v" + versionVal + " (Flash " + versionVal + ")";
    document.getElementById('hud-engine').textContent = asVersionVal === 2 ? "AVM2 (ActionScript 3.0)" : "AVM1 (ActionScript 1/2)";
    document.getElementById('hud-stage').textContent = width + " x " + height + " px";
    document.getElementById('hud-framerate').textContent = fpsVal + " FPS";
    document.getElementById('hud-frames').textContent = frameCountVal;
    document.getElementById('hud-bgcolor').textContent = hexColor;
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
