#!/bin/bash
# run.sh → chmod +x run.sh && ./run.sh

# --memory-swap is memory + swap so if we have 9GB Ram + 7 disk, its 9+7 = 16.
docker run -it --rm --memory=9G --memory-swap=16G --cpus="1.5" squirrelcommunicator:latest /bin/bash