# Squirrel Communicator

_Squirrel Communicator_ is a software project designed to deliver high-performance communication capabilities, leveraging C++ and complementary tools for modular design, efficient communication, and ease of deployment.

The application currently supports both **REST API** (using the CROWCPP library) and **WebSocket communication** (powered by uWebSockets) to provide stable and fastest available communication.

You can explore a **live working example** of this project at: [http://comm.sqrll.net/](http://comm.sqrll.net/).

---

## Current Functionality

Squirrel Communicator offers the following features:
- **Real-time chat functionality**: Seamlessly send and receive messages between users in real time.
- **Login and Registration**: Secure user authentication and onboarding.
- **User search**: Easily locate and select other users to create new chats.
- **User status display**: See the online or offline status of other users.
- **Typing indicator**: View when other users are actively typing messages in real time.
- **Automatic message loading**: Messages are automatically loaded when scrolling to the top of the chat history.

---

## Repository Overview

### Primary Technologies

- **C++**: The core of the project is written in C++ to ensure high performance and efficient execution of critical functionalities.
- **CROWCPP**: Provides the REST API functionality for user authentication, chat management, and data retrieval.
- **uWebSockets**: Enables real-time event-driven WebSocket communication for the chat application.
- **CMake**: Simplifies the build process and allows configuration for multiple platforms.
- **Docker**: Allows containerized setup and deployment in a consistent environment.
- **Build Scripts**: Preconfigured build scripts for Windows and Linux are available in the `configsrv` directory.

---

## How to Build

### Prerequisites

You will need the following tools to build and run the project:
- A C++ compiler (e.g., `g++` or `clang`).

View Dockerfile in docker directory for latest packages required on Linux (Fedora).

### Build Instructions

1. **Clone the repository**:
   ```bash
   git clone https://github.com/Przemek2122/SquirrelCommunicator.git
   cd SquirrelCommunicator
   ```
2. Download submodules
   First download 

4. **Use Build Scripts (Optional)**:
   If you're on **Windows** or **Linux**, you can use the pre-configured build scripts from the `configsrv` directory:
   - For Windows, execute the `.bat` file in the `configsrv` directory.
   - For Linux, execute the `.sh` file in the `configsrv` directory:
     ```bash
     cd configsrv
     ./build-linux.sh
     ```

5. **Manual Build**:
   If you'd like to build the project manually:
   - Change to the `ProjectServer` directory where the build files are located:
     ```bash
     cd ProjectServer
     ```
   - Create a build directory:
     ```bash
     mkdir build
     cd build
     ```
   - Use CMake to configure and compile:
     ```bash
     cmake ..
     make
     ```

   The compiled binaries will appear in the `ProjectServer/build` directory.

---

## Running with Docker

The project provides Docker support for simplified setup and consistent application environments.

### Pre-written Docker Scripts

Two scripts are available in the repository under the `docker` directory to simplify Docker operations:
- **`build_docker.sh`**: Builds a Docker image using the default cache layer.
- **`build_docker_clean.sh`**: Builds the Docker image without using the cache (forcing a clean build).

### Steps to Use the Scripts

1. Use the `build_docker.sh` script to build the Docker image:
   ```bash
   cd docker
   chmod +x build_docker.sh
   ./build_docker.sh
   ```

   This will build the Docker image with the tag `squirrelcommunicator:latest`.

2. Alternatively, use the `build_docker_clean.sh` script for a fresh build:
   ```bash
   chmod +x build_docker_clean.sh
   ./build_docker_clean.sh
   ```

3. Run the Docker container:
   ```bash
   docker run --rm -it squirrelcommunicator:latest
   ```

Using these predefined scripts ensures that Docker images are built properly without manually typing long commands.

---

## Mediasoup

For online audio communication, there is mediasoup server (by default it will use port 3000)


## Repository Limitations

- **Platform-Specific Utilities**:
    - The repository includes Shell scripts for UNIX/Linux and Batch files for Windows. Ensure you use the appropriate one for your platform.
- **Documentation**:
    - Some features and modules may require further documentation.
- **Experimental Features**:
    - Certain areas of the codebase may be under development or require testing in production-like environments.

---

## Contributing

Contributions are welcome! Here’s how you can contribute:
1. Fork the repository and create a feature branch.
2. Make your changes and add commits.
3. Open a pull request to propose your changes.

---

## License

For any usage or distribution inquiries, please contact the repository owner, [Przemek2122](https://github.com/Przemek2122).

---

## About the Author

The repository is maintained by [Przemek2122](https://github.com/Przemek2122). Feedback, ideas, and collaboration are warmly invited.
