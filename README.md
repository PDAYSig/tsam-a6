
# TSAM Assignment 6 - TCP Mail Server and Client

## Description

This project implements a TCP mail server and client using C++.

The server allows multiple clients to connect and exchange messages. Messages are stored on the server until they are retrieved.

The client connects to the server and allows the user to enter commands.

## Files

- `tsamserver.cpp` - Server implementation
- `tsamclient.cpp` - Client implementation
- `Makefile` - Compiles the programs
- `server.log` - Server activity log
- `client-output.txt` - Output from the client
- `README.md` - Project documentation

## Compilation

Compile both programs using:

```bash
make
```

Or compile them separately:

```bash
g++ -std=c++17 tsamserver.cpp -o tsamserver
g++ -std=c++17 tsamclient.cpp -o tsamclient
```

## Running the Programs

First, start the server:

```bash
./tsamserver 4044
```

Then open another terminal and start the client:

```bash
./tsamclient 127.0.0.1 4044
```

The client will ask for a username before accepting commands.

## Supported Commands

### SENDMSG

Send a message to another user.

Format:

```text
SENDMSG,<from>,<to>,<message>
```

Example:

```text
SENDMSG,Alice,Bob,Hello Bob
```

### GETMSG

Retrieve a message from the server.

Format:

```text
GETMSG,<username>
```

Example:

```text
GETMSG,Bob
```

If there are no messages, the server returns `EMPTY`.

### STATUSREQ

Request the number of unread messages for each user.

Format:

```text
STATUSREQ
```

Example response:

```text
Alice:2,Bob:1
```

If there are no unread messages, the server returns `EMPTY`.

## TCP Message Format

Messages are sent using the following frame format:

```text
SOH | Length (2 bytes) | STX | Message | ETX
```

- SOH = `0x01`
- Length = Total frame length in network byte order
- STX = `0x02`
- Message = Command or server response
- ETX = `0x03`

The maximum frame size is 5000 bytes.

The client handles partial TCP sends and receives.

## Logging

The server writes activity information to `server.log`.

To view the log:

```bash
cat server.log
```

To save client output to a file while also displaying it:

```bash
./tsamclient 127.0.0.1 4044 2>&1 | tee client-output.txt
```

## Closing the Client

Enter:

```text
Q
```

to close the client connection.

## Notes

- The server uses TCP sockets.
- The server uses `poll()` to handle multiple connections.
- The client validates commands before sending them.
- The client displays timestamps for commands and responses.
- The server and client must use the same command and framing protocol.
