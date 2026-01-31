const mediasoup = require('mediasoup');
const express = require('express');
const bodyParser = require('body-parser');

const app = express();
app.use(bodyParser.json());

let worker, router;

(async () => {
    worker = await mediasoup.createWorker({
        logLevel: 'warn',
        rtcMinPort: 40000,
        rtcMaxPort: 40100
    });

    router = await worker.createRouter({
        mediaCodecs: [
            {
                kind: 'audio',
                mimeType: 'audio/opus',
                clockRate: 48000,
                channels: 2
            }
        ]
    });

    console.log('Mediasoup worker created');

    app.get('/health', (req, res) => {
        res.json({ status: 'ok' });
    });

    app.post('/create-transport', async (req, res) => {
        try {
            const transport = await router.createWebRtcTransport({
                listenIps: [
                    {
                        ip: '0.0.0.0',
                        announcedIp: process.env.ANNOUNCED_IP || '127.0.0.1'
                    }
                ],
                enableUdp: true,
                enableTcp: false,
                preferUdp: true
            });

            res.json({
                id: transport.id,
                iceParameters: transport.iceParameters,
                iceCandidates: transport.iceCandidates,
                dtlsParameters: transport.dtlsParameters
            });
        } catch (error) {
            res.status(500).json({ error: error.message });
        }
    });

    app.listen(3000, () => {
        console.log('Mediasoup server listening on port 3000');
    });
})();
