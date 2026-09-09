const serviceUuid = '4fa8691a-1360-4c27-ba5c-057245417c92';
const dataUuid = '4fa8691b-1360-4c27-ba5c-057245417c92';
const commandUuid = '4fa8691c-1360-4c27-ba5c-057245417c92';
let dataCharacteristic, commandCharacteristic, transfer = '', transferType = '', chunkCount = 0, startTime = 0, expectedFileSize = 0, currentFileName = '';
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

        // Check if this is a brightness response
        if (chunk.startsWith('BRIGHTNESS:')) {
            const brightnessStr = chunk.substring(11).trim();
            const brightness0to255 = parseInt(brightnessStr, 10);
            const brightnessPercent = Math.round((brightness0to255 / 255) * 100);
            console.log(`[receivedData] Brightness response: ${brightness0to255} (0-255) = ${brightnessPercent}%`);
            $('brightnessSlider').value = brightnessPercent;
            $('brightnessValue').textContent = brightnessPercent + '%';
            return;
        }

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

        // Show live progress during file download
        if (transferType === 'file' && expectedFileSize > 0) {
            const bytesReceived = new TextEncoder().encode(transfer).length;
            const kbReceived = (bytesReceived / 1024).toFixed(1);
            const kbExpected = (expectedFileSize / 1024).toFixed(1);
            const percent = Math.min(100, Math.round((bytesReceived / expectedFileSize) * 100));
            setStatus(`${currentFileName} (${chunkCount} pkts | ${percent}% | ${kbReceived}/${kbExpected} KB)`);
        }

        if (chunk.includes('\x02')) {
            console.log('[receivedData] Transfer ended');

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
    const sorted = [...files].sort((a, b) => (b.name || b).localeCompare(a.name || a));
    for (const file of sorted) {
        const filePath = typeof file === 'string' ? file : file.name;
        const fileSize = typeof file === 'object' && file.size !== null ? formatFileSize(file.size) : '';
        const rawSize = typeof file === 'object' && file.size !== null ? file.size : 0;

        const li = document.createElement('li');
        const button = document.createElement('button');
        button.className = 'button-list';
        button.dataset.size = rawSize;
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
    const fileObj = Array.from($('fileList').querySelectorAll('.button-list')).find(btn => btn.querySelector('.file-name').textContent === name);
    expectedFileSize = fileObj ? parseInt(fileObj.dataset.size || '0') : 0;
    currentFileName = name;
    transfer = '';
    transferType = 'file';
    chunkCount = 0;
    console.log(`[openFile] Opening file: ${name}`);
    setStatus(`Loading ${name}...`);
    await commandCharacteristic.writeValue(new TextEncoder().encode('GET:' + name));
    console.log('[openFile] GET command sent');
}

function drawGraph(csv) {
    const rows = csv.trim().split(/\r?\n/).map(row => row.split(','));
    let points = rows.map(row => ({ time: row[0], pm1: +row[1], pm25: +row[2], pm10: +row[3] })).filter(row => Number.isFinite(row.pm10));
    if (!points.length) { setStatus('CSV has no readable rows'); return; }

    // Downsample to max 150 points via averaging
    if (points.length > 150) {
        const bucketSize = Math.ceil(points.length / 150);
        const downsampled = [];
        for (let i = 0; i < points.length; i += bucketSize) {
            const bucket = points.slice(i, i + bucketSize);
            downsampled.push({
                time: bucket[0].time,
                pm1: bucket.reduce((s, p) => s + p.pm1, 0) / bucket.length,
                pm25: bucket.reduce((s, p) => s + p.pm25, 0) / bucket.length,
                pm10: bucket.reduce((s, p) => s + p.pm10, 0) / bucket.length
            });
        }
        points = downsampled;
    }

    const pointWidth = 40, leftMargin = 70, rightMargin = 15, topMargin = 20, bottomMargin = 50;
    const width = Math.max(900, leftMargin + rightMargin + points.length * pointWidth);
    const canvas = $('graph');
    canvas.width = width;
    const context = canvas.getContext('2d');
    const height = canvas.height;
    context.clearRect(0, 0, width, height);

    const allValues = points.flatMap(p => [p.pm1, p.pm25, p.pm10]);
    const max = Math.max(10, ...allValues);
    const sorted = [...allValues].sort((a, b) => a - b);
    const median = sorted[Math.floor(sorted.length / 2)];
    const graphHeight = height - topMargin - bottomMargin;

    // Draw axes
    context.strokeStyle = '#aac0ca';
    context.lineWidth = 1;
    context.beginPath();
    context.moveTo(leftMargin, topMargin);
    context.lineTo(leftMargin, height - bottomMargin);
    context.lineTo(width - rightMargin, height - bottomMargin);
    context.stroke();

    // Y-axis labels (0, median, max)
    context.fillStyle = '#666';
    context.font = '12px monospace';
    context.textAlign = 'right';
    [0, median, max].forEach(val => {
        const y = height - bottomMargin - (val / max) * graphHeight;
        context.fillText(Math.round(val), leftMargin - 8, y + 4);
    });

    // Axis labels
    context.fillStyle = '#8fffe0';
    context.textAlign = 'center';
    context.font = '13px system-ui';
    context.fillText('Time (hours:minutes)', width / 2, height - 8);
    context.save();
    context.translate(15, height / 2);
    context.rotate(-Math.PI / 2);
    context.fillText('PM Concentration (µg/m³)', 0, 0);
    context.restore();

    // Plot lines
    context.lineWidth = 2;
    [['pm1', '#168aad'], ['pm25', '#ee6c4d'], ['pm10', '#293241']].forEach(([key, color]) => {
        context.strokeStyle = color;
        context.beginPath();
        points.forEach((point, index) => {
            const x = leftMargin + index * pointWidth;
            const y = height - bottomMargin - (point[key] / max) * graphHeight;
            index ? context.lineTo(x, y) : context.moveTo(x, y);
        });
        context.stroke();
    });

    // X-axis time labels (every ~8th point, showing HH:MM)
    context.fillStyle = '#d7fff4';
    context.font = '11px monospace';
    context.textAlign = 'center';
    const step = Math.ceil(points.length / 8);
    points.forEach((point, index) => {
        if (index % step === 0 || index === points.length - 1) {
            const x = leftMargin + index * pointWidth;
            const timeStr = point.time.split(' ')[1]?.slice(0, 5) || point.time;
            context.fillText(timeStr, x, height - 20);
        }
    });

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

        // Show brightness control
        $('brightnessControl').style.display = 'block';

        // Request current brightness from ESP32
        await commandCharacteristic.writeValue(new TextEncoder().encode('GetBrightness'));
        console.log('[ConnectESP32] GetBrightness command sent');

        // Request file list from ESP32
        transferType = 'list';
        await commandCharacteristic.writeValue(new TextEncoder().encode('LIST'));
        console.log('[ConnectESP32] LIST command sent');


    } catch (error) {
        console.error('[ConnectESP32] Error:', error);
        setStatus("Encountered error connecting: " + error.message);
    }
};

// Brightness slider handler
$('brightnessSlider').addEventListener('input', async (event) => {
    const brightnessPercent = parseInt(event.target.value, 10);
    $('brightnessValue').textContent = brightnessPercent + '%';

    // Convert percentage (1-100) to 0-255
    const brightness0to255 = Math.round((brightnessPercent / 100) * 255);

    console.log(`[brightnessSlider] Sending SetBrightness: ${brightnessPercent}% = ${brightness0to255} (0-255)`);

    if (commandCharacteristic) {
        try {
            await commandCharacteristic.writeValue(new TextEncoder().encode(`SetBrightness:${brightness0to255}`));
            console.log('[brightnessSlider] SetBrightness command sent');
        } catch (error) {
            console.error('[brightnessSlider] Error sending brightness:', error);
        }
    }
});
