#!/bin/bash
# run.sh → chmod +x run.sh && ./run.sh

# Run app docker until stopped
# --memory-swap is memory + swap so if we have 9GB Ram + 7 disk, its 9+7 = 16.
docker run -d --restart unless-stopped -it --memory=9G --memory-swap=16G --cpus=2 \
--network host \
-v /etc/letsencrypt:/etc/letsencrypt \
-v /var/repos:/var/repos \
squirrelcommunicator:latest /bin/bash
