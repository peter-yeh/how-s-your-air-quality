const serviceUuid = '4fa8691a-1360-4c27-ba5c-057245417c92';
const dataUuid = '4fa8691b-1360-4c27-ba5c-057245417c92';
const commandUuid = '4fa8691c-1360-4c27-ba5c-057245417c92';
let dataCharacteristic, commandCharacteristic, transfer = '', transferType = '', chunkCount = 0, startTime = 0;
const $ = id => document.getElementById(id);

function setStatus(message) { $('status').textContent = message; }

function handleData(event) {
    try {
        const chunk = new TextDecoder().decode(event.target.value);
        chunkCount++;
        console.log(`[Chunk ${chunkCount}] Received ${chunk.length} bytes: "${chunk.substring(0, 50)}${chunk.length > 50 ? '...' : ''}"`);
        transfer += chunk;

        // Send acknowledgment to backend to enable flow control
        commandCharacteristic.writeValue(new TextEncoder().encode('ACK'));

        // Show real-time progress
        const totalBytes = transfer.length;
        console.log(`[Progress] Total received: ${totalBytes} bytes in ${chunkCount} chunks`);

        if (transfer.includes('BEGIN LIST\n') && transfer.includes('END LIST\n')) {
            const files = transfer.split('BEGIN LIST\n')[1].split('END LIST\n')[0].split(/\r?\n/).filter(Boolean);
            console.log(`[Complete] File list received: ${files.length} files, ${totalBytes} bytes`);
            renderFileList(files);
            chunkCount = 0;
            transfer = '';
        } else if (transfer.includes('BEGIN CSV\n') && transfer.includes('END CSV\n')) {
            const csv = transfer.split('BEGIN CSV\n')[1].split('END CSV\n')[0];
            const rows = csv.trim().split(/\r?\n/).filter(Boolean).length;
            console.log(`[Complete] CSV file received: ${rows} rows, ${totalBytes} bytes, ${chunkCount} chunks`);
            setStatus(`Received: ${totalBytes} bytes in ${chunkCount} chunks (${rows} data rows)`);
            transfer = '';
            chunkCount = 0;
            if (!csv.startsWith('ERROR FILE')) drawGraph(csv);
            else setStatus('Could not open that CSV file');
        } else if (transfer.includes('BEGIN')) {
            // Transfer in progress, show live status
            setStatus(`Receiving... ${totalBytes} bytes in ${chunkCount} chunks`);
        }
    } catch (e) {
        console.error('[handleData] Error processing chunk:', e);
    }
}

function renderFileList(files) {
    $('fileList').innerHTML = '';
    for (const name of files) {
        const li = document.createElement('li');
        const button = document.createElement('button');
        button.textContent = name;
        button.onclick = () => openFile(name);
        li.appendChild(button);
        $('fileList').appendChild(li);
    }
}

async function openFile(name) {
    if (!name.toLowerCase().endsWith('.csv')) return;
    transfer = '';
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

$('connect').onclick = async () => {
    try {
        console.info('Connecting to Bluetooth device...');
        if (!navigator.bluetooth) throw new Error('Web Bluetooth is unavailable. Use Chrome or Edge over HTTPS.');
        const device = await navigator.bluetooth.requestDevice({ filters: [{ services: [serviceUuid] }] });
        console.log('[Connection] Device selected:', device.name);

        const server = await device.gatt.connect();
        console.log('[Connection] GATT server connected');

        const service = await server.getPrimaryService(serviceUuid);
        console.log('[Connection] Service found');

        dataCharacteristic = await service.getCharacteristic(dataUuid);
        commandCharacteristic = await service.getCharacteristic(commandUuid);
        console.log('[Connection] Characteristics obtained');

        // Remove any existing listeners
        dataCharacteristic.removeEventListener('characteristicvaluechanged', handleData);

        // Start notifications and attach listener
        await dataCharacteristic.startNotifications();
        console.log('[Connection] Notifications started');

        dataCharacteristic.addEventListener('characteristicvaluechanged', handleData);
        console.log('[Connection] Event listener attached');

        setStatus(`Connected to ${device.name || 'ESP32'}`);
        console.info('Connected to Bluetooth device.');

        // Send LIST command
        console.log('[Connection] Sending LIST command...');
        await commandCharacteristic.writeValue(new TextEncoder().encode('LIST'));
        console.log('[Connection] LIST command sent');

    } catch (error) {
        console.error('[Connection] Error:', error);
        setStatus("Encountered error connecting: " + error.message);
    }
};
