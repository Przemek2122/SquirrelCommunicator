# Docker and `.env` File Configuration

This document explains how to properly configure the environment variables required to run the connected services:
1. **C++ Backend** (REST CROWCPP & UWebSockets)
2. **Go Voice Service** (SquirrelCommunicatorVoice)

## 1.`.env.backend`

```
# Database connections env vars
SQRLL_COMM_DB_HOST=127.0.0.1
SQRLL_COMM_DB_PORT=3306
SQRLL_COMM_DB_DBNAME=sqrllapitest
SQRLL_COMM_DB_USER=commapisqrllusertest
SQRLL_COMM_DB_PASSWORD=

# Brevo API key
SQRLL_COMM_MAIL_API_KEY=
```

## 2.`.env.voice`
```
# Address for backend app
SQRLL_VOICE_ADDRESS=localhost

# The port the Go service runs on for Voice and Backend app server
SQRLL_VOICE_PORT=8082

# The main password for communicating with the backend (Change this!)
SQRLL_VOICE_API_KEY=
```
