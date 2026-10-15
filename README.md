
# TSAM Assignment 6 — TCP Mail Client and Server

A TCP mail system written in C++ using POSIX sockets.

## Compile

```bash
make
```

## Run

Start the server in one terminal:

```bash
./tsamserver 4044
```

Start the client in another terminal:

```bash
./tsamclient 127.0.0.1 4044
```

## Commands

| Command | Description |
| --- | --- |
| `SENDMSG,<from>,<to>,<message>` | Send a message to a user. |
| `GETMSG,<username>` | Retrieve a user's next message. |
| `STATUSREQ` | Request the number of waiting messages for each user. |
| `Q` | Quit the client (local command). |

Example commands:

```text
SENDMSG,Alice,Bob,Hello Bob!
GETMSG,Bob
STATUSREQ
```

## Protocol

Messages use the binary frame format:

```text
<SOH><length: 2 bytes><STX><message><ETX>
```

The length includes the framing bytes and is stored in network byte order. Maximum frame size: **5000 bytes**.

## Submission

Include the source code, Makefile, README, server log, client output, and Wireshark capture required by the assignment.
