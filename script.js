const serviceUuid = '4fa8691a-1360-4c27-ba5c-057245417c92';
const dataUuid = '4fa8691b-1360-4c27-ba5c-057245417c92';
const commandUuid = '4fa8691c-1360-4c27-ba5c-057245417c92';
let dataCharacteristic, commandCharacteristic, transfer = '', transferType = '', chunkCount = 0, startTime = 0;
const $ = id => document.getElementById(id);

function setStatus(message) { $('status').textContent = message; }

function formatFileSize(bytes) {
    if (bytes === null || bytes === undefined || isNaN(bytes)) return '';
    const mb = Number(bytes) / (1024 * 1024);
    return `${mb.toFixed(2)} MB`;
}

function receivedData(event) {
    try {
        console.log(`[receivedData] Event received with ${event.target.value.byteLength} bytes`);
        const chunk = new TextDecoder().decode(event.target.value);

        // \x01 = start of transfer, \x02 = end of transfer
        if (chunk.includes('\x01')) {
            transfer = '';
            chunkCount = 0;
            startTime = Date.now();
            console.log('[receivedData] Transfer started');
        }

        // Strip control characters and accumulate
        transfer += chunk.replace(/[\x01\x02]/g, '');
        chunkCount++;

        if (chunk.includes('\x02')) {
            const elapsed = ((Date.now() - startTime) / 1000).toFixed(1);
            console.log(`[receivedData] Transfer complete: ${chunkCount} chunks in ${elapsed}s`);

            const payload = transfer.trim();
            const entries = payload.split('\n').map(s => s.trim()).filter(Boolean);
            const parsedFiles = entries.map(entry => {
                const parts = entry.split('|');
                return {
                    name: parts[0].trim(),
                    size: parts.length > 1 ? parseInt(parts[1].trim(), 10) : null
                };
            }).filter(f => f.name.toLowerCase().endsWith('.csv'));

            const isFileList = transferType === 'list' || (transferType !== 'file' && parsedFiles.length > 0);

            if (isFileList) {
                console.log(`[receivedData] File list received:`, parsedFiles);
                renderFileList(parsedFiles);
                setStatus(`Found ${parsedFiles.length} CSV file(s)`);
            } else {
                // Treat as CSV file content
                console.log('[receivedData] CSV content received, drawing graph');
                drawGraph(payload);
            }

            transfer = '';
            transferType = '';
        }



    } catch (e) {
        console.error('[receivedData] Error processing chunk:', e);
    }
}

function renderFileList(files) {
    $('fileList').innerHTML = '';
    for (const file of files) {
        const filePath = typeof file === 'string' ? file : file.name;
        const fileSize = typeof file === 'object' && file.size !== null ? formatFileSize(file.size) : '';

        const li = document.createElement('li');
        const button = document.createElement('button');
        button.className = 'button-list';
        button.onclick = () => openFile(filePath);

        const nameSpan = document.createElement('span');
        nameSpan.className = 'file-name';
        nameSpan.textContent = filePath;
        button.appendChild(nameSpan);

        if (fileSize) {
            const sizeSpan = document.createElement('span');
            sizeSpan.className = 'file-size';
            sizeSpan.textContent = fileSize;
            button.appendChild(sizeSpan);
        }

        li.appendChild(button);
        $('fileList').appendChild(li);
    }
}

async function openFile(name) {
    if (!name.toLowerCase().endsWith('.csv')) return;
    transfer = '';
    transferType = 'file';
    chunkCount = 0;
    console.log(`[openFile] Opening file: ${name}`);
    setStatus(`Loading ${name}...`);
    console.log('[openFile] Sending GET command');
    await commandCharacteristic.writeValue(new TextEncoder().encode('GET:' + name));
    console.log('[openFile] GET command sent');
}

function drawGraph(csv) {
    const rows = csv.trim().split(/\r?\n/).map(row => row.split(','));
    const points = rows.map(row => ({ time: row[0], pm1: +row[1], pm25: +row[2], pm10: +row[3] })).filter(row => Number.isFinite(row.pm10));
    if (!points.length) { setStatus('CSV has no readable rows'); return; }

    const pointWidth = 40, leftMargin = 50, rightMargin = 15, topMargin = 15, bottomMargin = 35;
    const width = Math.max(900, leftMargin + rightMargin + points.length * pointWidth);
    const canvas = $('graph');
    canvas.width = width;
    const context = canvas.getContext('2d');
    const height = canvas.height;
    context.clearRect(0, 0, width, height);

    const max = Math.max(10, ...points.flatMap(point => [point.pm1, point.pm25, point.pm10]));
    context.strokeStyle = '#aac0ca';
    context.beginPath();
    context.moveTo(leftMargin, topMargin);
    context.lineTo(leftMargin, height - bottomMargin);
    context.lineTo(width - rightMargin, height - bottomMargin);
    context.stroke();

    [['pm1', '#168aad'], ['pm25', '#ee6c4d'], ['pm10', '#293241']].forEach(([key, color]) => {
        context.strokeStyle = color;
        context.beginPath();
        points.forEach((point, index) => {
            const x = leftMargin + index * pointWidth, y = height - bottomMargin - point[key] * (height - topMargin - bottomMargin) / max;
            index ? context.lineTo(x, y) : context.moveTo(x, y);
        });
        context.stroke();
    });

    context.fillStyle = '#d7fff4';
    context.fillText(`0 - ${max.toFixed(0)} ug/m3`, 5, 15);
    points.forEach((point, index) => context.fillText(point.time || '', leftMargin + index * pointWidth - 15, height - 12));
    setStatus(`CSV loaded: ${points.length} reading(s)`);
}


$('ConnectESP32').onclick = async () => {
    try {
        console.info('[ConnectESP32] Connecting to Bluetooth device...please select a device');
        if (!navigator.bluetooth) throw new Error('Web Bluetooth is unavailable. Use Chrome or Edge over HTTPS.');
        const device = await navigator.bluetooth.requestDevice({ filters: [{ services: [serviceUuid] }] });
        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(serviceUuid);

        dataCharacteristic = await service.getCharacteristic(dataUuid);
        commandCharacteristic = await service.getCharacteristic(commandUuid);

        // Remove any existing listeners
        dataCharacteristic.removeEventListener('characteristicvaluechanged', receivedData);

        // Start notifications and attach listener
        await dataCharacteristic.startNotifications();
        dataCharacteristic.addEventListener('characteristicvaluechanged', receivedData);

        setStatus(`Connected to ${device.name || 'ESP32'}, fetching files...`);
        console.info('[ConnectESP32] Connected to Bluetooth device.');

        // Request file list from ESP32
        transferType = 'list';
        await commandCharacteristic.writeValue(new TextEncoder().encode('LIST'));
        console.log('[ConnectESP32] LIST command sent');


    } catch (error) {
        console.error('[ConnectESP32] Error:', error);
        setStatus("Encountered error connecting: " + error.message);
    }
};

