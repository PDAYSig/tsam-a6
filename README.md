
# TSAM Assignment 6 - TCP Mail Server and Client

## Description

This project implements a TCP mail server and client in C++ using POSIX sockets.

The server supports multiple clients using `poll()`. Clients can send messages to other users, retrieve unread messages, and request the number of stored messages.

Messages are stored on the server until they are retrieved.

## Project Files

| File | Description |
|------|-------------|
| `tsamserver.cpp` | TCP server implementation |
| `tsamclient.cpp` | TCP client implementation |
| `Makefile` | Compiles the server and client |
| `server.log` | Server activity log |
| `client-output.txt` | Recorded client output |
| `README.md` | Project documentation |

## Compilation

Compile both programs using:

```bash
make
```

Alternatively, compile them manually:

```bash
g++ -std=c++17 tsamserver.cpp -o tsamserver
g++ -std=c++17 tsamclient.cpp -o tsamclient
```

## Running the Programs

**1. Start the server**

Open a terminal and run:

```bash
./tsamserver 4044
```

**2. Start the client**

Open another terminal and run:

```bash
./tsamclient 127.0.0.1 4044
```

The IP address `127.0.0.1` connects to a server running on the same computer. Port `4044` is an example and can be changed.

After connecting, the client asks for a username. Once the username is sent, the client can enter commands.

Multiple clients can connect to the server at the same time.

## Supported Commands

### SENDMSG

Send a message to another user.

**Format:**

```text
SENDMSG,<from>,<to>,<message>
```

**Example:**

```text
SENDMSG,Alice,Bob,Hello Bob
```

The server stores the message for Bob until it is retrieved.

### GETMSG

Retrieve an unread message for a user.

**Format:**

```text
GETMSG,<username>
```

**Example:**

```text
GETMSG,Bob
```

If no unread messages are available, the server returns `EMPTY`.

### STATUSREQ

Request the number of unread messages stored for each user.

**Format:**

```text
STATUSREQ
```

**Example response:**

```text
Alice:2,Bob:1
```

If no unread messages are stored, the server returns `EMPTY`.

## TCP Message Format

All commands and responses use the following frame structure:

```text
SOH | Length (2 bytes) | STX | Message | ETX
```

| Field | Description |
|-------|-------------|
| SOH | Start of header (`0x01`) |
| Length | Total frame length in network byte order |
| STX | Start of text (`0x02`) |
| Message | Command or response |
| ETX | End of text (`0x03`) |

The maximum frame size is **5000 bytes**, including the header and control bytes.

The client handles partial TCP sends and receives to ensure complete frames are transmitted.

## Logging

The server records activity in `server.log`, including:

- Server startup and shutdown
- Client connections and disconnections
- Received commands
- Sent responses
- Message operations
- Connection and protocol errors

Log entries include timestamps in the format:

```text
DD/MM/YYYY HH:MM:SS
```

**Example:**

```text
15/10/2026 23:10:00 | Server started
15/10/2026 23:10:05 | Client connected
15/10/2026 23:10:15 | Command received: STATUSREQ
15/10/2026 23:10:15 | Response sent: Bob:1
15/10/2026 23:10:30 | Client disconnected
```

To view the server log:

```bash
cat server.log
```

### Saving Client Output

To display client output and save it to `client-output.txt` at the same time, run:

```bash
./tsamclient 127.0.0.1 4044 2>&1 | tee client-output.txt
```

The client displays timestamps for commands and server responses.

## Closing the Programs

To disconnect the client, enter:

```text
Q
```

To stop the server, press `Ctrl+C` in the server terminal.

## Implementation

- **Language:** C++
- **Transport protocol:** TCP
- **Networking:** POSIX sockets
- **Multiple clients:** `poll()`
- **Message framing:** SOH, length, STX, payload, ETX
- **Message storage:** Server-side unread message storage
- **Platform:** Linux / Ubuntu (WSL)
