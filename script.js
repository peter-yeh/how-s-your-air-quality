const serviceUuid = '4fa8691a-1360-4c27-ba5c-057245417c92';
const dataUuid = '4fa8691b-1360-4c27-ba5c-057245417c92';
const commandUuid = '4fa8691c-1360-4c27-ba5c-057245417c92';
let dataCharacteristic, commandCharacteristic, transfer = '', transferType = '', chunkCount = 0, startTime = 0, expectedFileSize = 0, currentFileName = '';
let airQualityChart = null;
const $ = id => document.getElementById(id);
const graphModeLabels = ['Seconds', 'Minutes', 'Hours'];

function setStatus(message) { $('status').textContent = message; }

function formatFileSize(bytes) {
    if (bytes === null || bytes === undefined || isNaN(bytes)) return '';
    const mb = Number(bytes) / (1024 * 1024);
    return `${mb.toFixed(2)} MB`;
}

function applySettings(brightness0to255, graphMode) {
    const brightnessPercent = Math.round((brightness0to255 / 255) * 100);
    const brightnessSlider = $('brightnessSlider');
    brightnessSlider.value = brightnessPercent;
    brightnessSlider.disabled = false;
    $('brightnessValue').textContent = brightnessPercent + '%';

    document.querySelectorAll('input[name="graphMode"]').forEach(radio => {
        radio.disabled = false;
        radio.checked = radio.value === String(graphMode);
    });
    $('graphModeValue').textContent = graphModeLabels[graphMode];

    console.log(`[applySettings] Brightness: ${brightness0to255} (0-255) = ${brightnessPercent}%, graph mode: ${graphModeLabels[graphMode]}`);
}

function receivedData(event) {
    try {
        console.log(`[receivedData] Event received with ${event.target.value.byteLength} bytes`);
        const chunk = new TextDecoder().decode(event.target.value);

        if (chunk.startsWith('SETTINGS:')) {
            const values = chunk.substring('SETTINGS:'.length).trim().split(',');
            const brightness0to255 = Number(values[0]);
            const graphMode = Number(values[1]);

            if (Number.isInteger(brightness0to255) && brightness0to255 >= 0 && brightness0to255 <= 255 &&
                Number.isInteger(graphMode) && graphMode >= 0 && graphMode <= 2) {
                applySettings(brightness0to255, graphMode);
            } else {
                console.error(`[receivedData] Invalid settings response: ${chunk}`);
            }
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

    const readingCount = points.length;
    if (points.length > 600) {
        const bucketSize = Math.ceil(points.length / 600);
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

    if (typeof Chart === 'undefined') {
        setStatus('Chart library unavailable. Check the network connection and reload.');
        return;
    }

    if (airQualityChart) airQualityChart.destroy();

    const cssColor = name => getComputedStyle(document.documentElement).getPropertyValue(name).trim();
    const chartColors = {
        pm1: cssColor('--chart-pm1') || '#29d9ad',
        pm1Fill: cssColor('--chart-pm1-fill') || 'rgba(41, 217, 173, .12)',
        pm25: cssColor('--chart-pm25') || '#ff8068',
        pm25Fill: cssColor('--chart-pm25-fill') || 'rgba(255, 128, 104, .12)',
        pm10: cssColor('--chart-pm10') || '#f0c15b',
        pm10Fill: cssColor('--chart-pm10-fill') || 'rgba(240, 193, 91, .12)',
        text: cssColor('--chart-text') || '#91a7a2',
        grid: cssColor('--chart-grid') || 'rgba(132, 179, 167, .16)',
        tooltip: cssColor('--chart-tooltip') || '#0b1715',
        tooltipBorder: cssColor('--chart-tooltip-border') || '#23534a'
    };

    const formatTimeLabel = value => {
        const match = value.match(/(?:T|\s)(\d{1,2}:\d{2})/);
        return match ? match[1] : value;
    };
    const labels = points.map(point => formatTimeLabel(point.time));
    const allValues = points.flatMap(point => [point.pm1, point.pm25, point.pm10]);
    const suggestedMax = Math.ceil(Math.max(10, ...allValues) * 1.12 / 10) * 10;
    const canvas = $('graph');

    airQualityChart = new Chart(canvas, {
        type: 'line',
        data: {
            labels,
            datasets: [
                {
                    label: 'PM1.0',
                    data: points.map(point => point.pm1),
                    borderColor: chartColors.pm1,
                    backgroundColor: chartColors.pm1Fill,
                    borderWidth: 2,
                    pointRadius: 0,
                    pointHoverRadius: 5,
                    pointHoverBackgroundColor: '#ffffff',
                    pointHoverBorderWidth: 2,
                    tension: 0.32,
                    fill: false,
                    spanGaps: true
                },
                {
                    label: 'PM2.5',
                    data: points.map(point => point.pm25),
                    borderColor: chartColors.pm25,
                    backgroundColor: chartColors.pm25Fill,
                    borderWidth: 2.5,
                    pointRadius: 0,
                    pointHoverRadius: 5,
                    pointHoverBackgroundColor: '#ffffff',
                    pointHoverBorderWidth: 2,
                    tension: 0.32,
                    fill: false,
                    spanGaps: true
                },
                {
                    label: 'PM10',
                    data: points.map(point => point.pm10),
                    borderColor: chartColors.pm10,
                    backgroundColor: chartColors.pm10Fill,
                    borderWidth: 2,
                    pointRadius: 0,
                    pointHoverRadius: 5,
                    pointHoverBackgroundColor: '#ffffff',
                    pointHoverBorderWidth: 2,
                    tension: 0.32,
                    fill: false,
                    spanGaps: true
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: {
                duration: 550,
                easing: 'easeOutQuart'
            },
            interaction: {
                mode: 'index',
                intersect: false
            },
            layout: {
                padding: {
                    top: 4,
                    right: 10,
                    bottom: 2,
                    left: 4
                }
            },
            plugins: {
                legend: {
                    position: 'top',
                    align: 'start',
                    labels: {
                        color: chartColors.text,
                        usePointStyle: true,
                        pointStyle: 'line',
                        boxWidth: 28,
                        padding: 22,
                        font: {
                            family: 'Aptos, Segoe UI, sans-serif',
                            size: 12,
                            weight: '600'
                        }
                    }
                },
                tooltip: {
                    backgroundColor: chartColors.tooltip,
                    borderColor: chartColors.tooltipBorder,
                    borderWidth: 1,
                    titleColor: '#effffb',
                    bodyColor: '#d4e7e2',
                    padding: 12,
                    displayColors: true,
                    callbacks: {
                        title: items => points[items[0].dataIndex]?.time || '',
                        label: context => ` ${context.dataset.label}: ${Number(context.raw).toFixed(1)} µg/m³`
                    }
                }
            },
            scales: {
                x: {
                    border: {
                        display: false
                    },
                    grid: {
                        display: false
                    },
                    ticks: {
                        color: chartColors.text,
                        maxTicksLimit: 8,
                        maxRotation: 0,
                        padding: 8,
                        font: {
                            family: 'Aptos, Segoe UI, sans-serif',
                            size: 11
                        }
                    }
                },
                y: {
                    beginAtZero: true,
                    suggestedMax,
                    border: {
                        display: false
                    },
                    grid: {
                        color: chartColors.grid,
                        drawTicks: false
                    },
                    ticks: {
                        color: chartColors.text,
                        padding: 10,
                        font: {
                            family: 'Aptos, Segoe UI, sans-serif',
                            size: 11
                        }
                    },
                    title: {
                        display: true,
                        text: 'µg/m³',
                        color: chartColors.text,
                        font: {
                            family: 'Aptos, Segoe UI, sans-serif',
                            size: 11,
                            weight: '600'
                        }
                    }
                }
            }
        }
    });

    $('chartSummary').textContent = `${readingCount.toLocaleString()} readings${readingCount > points.length ? ` · ${points.length.toLocaleString()} plotted` : ''}`;
    setStatus(`CSV loaded: ${readingCount.toLocaleString()} reading(s)`);
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

        setStatus(`Connected to ${device.name || 'ESP32'}, fetching settings and files...`);
        console.info('[ConnectESP32] Connected to Bluetooth device.');

        // Show brightness control
        $('brightnessControl').style.display = 'block';

        // Request current display settings from ESP32
        await commandCharacteristic.writeValue(new TextEncoder().encode('GetSettings'));
        console.log('[ConnectESP32] GetSettings command sent');

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

// Graph mode radio buttons handler
document.querySelectorAll('input[name="graphMode"]').forEach(radio => {
    radio.addEventListener('change', async (event) => {
        const mode = event.target.value;
        console.log(`[graphMode] Sending SetGraphMode: ${mode}`);
        if (commandCharacteristic) {
            try {
                await commandCharacteristic.writeValue(new TextEncoder().encode(`SetGraphMode:${mode}`));
                console.log('[graphMode] SetGraphMode command sent');
            } catch (error) {
                console.error('[graphMode] Error sending graph mode:', error);
            }
        }
    });
});
