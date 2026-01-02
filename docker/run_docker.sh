#!/bin/bash
# run.sh → chmod +x run.sh && ./run.sh

# Run app docker until stopped
# --memory-swap is memory + swap so if we have 9GB Ram + 7 disk, its 9+7 = 16.
docker run -d --restart unless-stopped -p 8080-8081:8080-8081 -it --memory=9G --memory-swap=16G --cpus=2 \
-v /etc/letsencrypt/live/comm.sqrll.net/fullchain.pem:/etc/letsencrypt/live/comm.sqrll.net/fullchain.pem \
-v /etc/letsencrypt/live/comm.sqrll.net/privkey.pem:/etc/letsencrypt/live/comm.sqrll.net/privkey.pem \
-v /var/repos \
squirrelcommunicator:latest /bin/bash