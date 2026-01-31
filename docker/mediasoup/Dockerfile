FROM node:18-slim

RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY package.json .
RUN npm install
COPY server.js .
EXPOSE 3000 40000-40100/udp
CMD ["node", "server.js"]
