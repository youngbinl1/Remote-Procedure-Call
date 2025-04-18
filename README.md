# Custom RPC System in C

This project implements a custom Remote Procedure Call (RPC) system in the C programming language. Designed with a client-server architecture, this RPC framework allows function calls to be executed remotely as though they were local, enabling seamless communication between distributed applications.

## 📖 Overview

Remote Procedure Call (RPC) is a powerful abstraction in distributed computing that allows programs to invoke procedures located on remote systems as if they were part of the local application. This project builds a simple yet effective RPC mechanism from scratch—no third - party RPC libraries used.

The system is implemented using:
- Low-level sockets for network communication.
- A custom application layer protocol.
- A clean API that supports modular testing and integration with external client/server applications.


## 📁 Files and Structure
rpc_project/
├── rpc.c               // Core implementation of the RPC system
├── rpc.h               // Header file with API declarations
├── Makefile            // Build configuration for compiling the RPC library
├── client.c (optional) // Example client implementation (for testing)
├── server.c (optional) // Example server implementation (for testing)

### Building the Project

Use the provided Makefile:
bash
make

This will compile `rpc.c` and generate the necessary object files for linking with your own client/server code.


## 🔌 Implementation Notes

- The RPC system is implemented from scratch, using raw sockets for communication.
- The system strictly adheres to a specified API (defined in `rpc.h`), which must remain unchanged to ensure compatibility with external test harnesses.
