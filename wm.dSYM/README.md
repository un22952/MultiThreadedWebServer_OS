# Multi-Threaded HTTP Web Server and Client

## Overview

This project implements a multi-threaded HTTP server and a multi-threaded client in C using POSIX sockets and POSIX threads.

The project contains two main components:

* **`webserver_multi`** — a multi-threaded HTTP server that uses a producer-consumer architecture.
* **`client`** — a multi-threaded HTTP client that can generate multiple concurrent requests to the server.

The multi-threaded server uses a fixed-size request buffer, mutex synchronization, and semaphores to coordinate a listener thread with multiple worker threads. It also includes a thread-monitoring mechanism that attempts to recreate worker threads if they terminate unexpectedly.

## Features

### Multi-Threaded Server

The server provides:

* TCP socket-based HTTP connections
* One listener thread for accepting incoming connections
* Multiple worker threads for processing requests
* A fixed-size request buffer with a capacity of 100 connections
* Producer-consumer synchronization
* POSIX mutex for protecting the shared request buffer
* POSIX semaphores for tracking empty and occupied buffer slots
* Worker-thread monitoring and recreation
* Configurable listening port and number of worker threads

The listener accepts incoming TCP connections and places their socket descriptors into the shared buffer. Worker threads remove connections from the buffer and pass them to the existing `process()` function.

### Multi-Threaded Client

The client can:

* Connect to a server using an IP address or hostname
* Send HTTP GET requests
* Create multiple client threads
* Retrieve a specified web page
* Measure the execution time of requests
* Report the number of bytes received by each thread
* Count failed requests
* Support up to 100 concurrent client threads

The default number of client threads is 10, and the default requested page is `/`.

---

## Project Structure

```text
.
├── Makefile
├── webserver.c
├── webserver.h
├── net.c
├── webserver_multi.c
├── client.c
└── README.md
```

### Files

| File                | Description                                              |
| ------------------- | -------------------------------------------------------- |
| `webserver_multi.c` | Multi-threaded HTTP server implementation                |
| `client.c`          | Multi-threaded HTTP client                               |
| `webserver.c`       | HTTP request-processing functionality used by the server |
| `net.c`             | Networking/request support used by the server            |
| `webserver.h`       | Header file shared by the server components              |
| `Makefile`          | Builds the server and client executables                 |
| `README.md`         | Project documentation                                    |

> **Note:** `webserver_multi.c` includes `webserver.h` and calls `process()`, while the Makefile also requires `webserver.c` and `net.c`. These files therefore need to be present in the project directory when building the server.

---

## Building the Project

The project uses GCC and POSIX threads.

Run:

```bash
make
```

The Makefile builds three executables:

```text
webserver
webserver_multi
client
```

The compilation commands use:

```text
gcc -g -lpthread
```

The `clean` target removes the generated executables:

```bash
make clean
```

The build targets and compiler settings are defined in the provided Makefile.

---

## Running the Multi-Threaded Server

The server expects two command-line arguments:

```bash
./webserver_multi <PORT> <NUMBER_OF_THREADS>
```

For example:

```bash
./webserver_multi 8080 10
```

This starts the server on port `8080` with 10 worker threads.

The server accepts ports from approximately `2001` through `49999`, according to the argument validation in the implementation. The number of worker threads is capped at 100.

When successfully started, the server creates the requested worker threads and then creates a listener thread.

---

## Running the Client

The client syntax is:

```bash
./client <host> <port> <number_of_threads> [page]
```

For example, if the server is running locally:

```bash
./client 127.0.0.1 8080 10
```

To request a specific page:

```bash
./client 127.0.0.1 8080 20 index.html
```

### Arguments

| Argument            | Description                          |
| ------------------- | ------------------------------------ |
| `host`              | Server IP address or hostname        |
| `port`              | Server TCP port                      |
| `number_of_threads` | Number of concurrent client requests |
| `page`              | Page to request; defaults to `/`     |

If the number of threads is not specified, the client uses 10 threads. The maximum is 100 threads.

---

## Server Architecture

The multi-threaded server follows a **producer-consumer model**.

```text
                   Incoming TCP Connections
                            |
                            v
                    +---------------+
                    |    Listener   |
                    |     Thread    |
                    +-------+-------+
                            |
                            v
                 +-----------------------+
                 |   Shared Request      |
                 |       Buffer          |
                 |    Capacity = 100     |
                 +-----------------------+
                    |       |       |
                    v       v       v
                 Worker  Worker  Worker
                 Thread  Thread  Thread
                    |       |       |
                    +-------+-------+
                            |
                            v
                       process(s)
```

### Listener Thread

The listener acts as the **producer**.

It:

1. Creates a TCP socket.
2. Binds the socket to the configured port.
3. Starts listening for incoming connections.
4. Accepts a connection.
5. Waits for an available buffer slot.
6. Places the socket descriptor into the shared buffer.
7. Signals that a request is available.

The request buffer uses a circular-buffer structure:

```c
buf[in] = s;
in = (in + 1) % MAX_REQUEST;
```

with `MAX_REQUEST` set to 100.

### Worker Threads

The worker threads act as **consumers**.

Each worker:

1. Waits until a request is available.
2. Locks the shared buffer.
3. Removes a socket descriptor.
4. Updates the buffer index.
5. Releases the mutex.
6. Signals that an empty buffer slot is available.
7. Processes the connection using `process(s)`.

This allows multiple HTTP requests to be processed concurrently.

---

## Synchronization

The server uses both a mutex and semaphores to coordinate access to the shared request buffer.

### Mutex

A POSIX mutex protects modifications to the shared buffer and its indexes:

```c
pthread_mutex_lock(&lock);
...
pthread_mutex_unlock(&lock);
```

This prevents multiple threads from modifying the buffer simultaneously.

### Semaphores

Two named semaphores are used:

```text
/sem_empty
/sem_full
```

`sem_empty` tracks available positions in the request buffer.

It is initialized to:

```text
100
```

`sem_full` tracks the number of requests currently waiting in the buffer and is initialized to:

```text
0
```

The listener waits on `sem_empty` before inserting a connection and posts `sem_full` after inserting one. Worker threads perform the reverse operation.

Conceptually:

```text
Listener                    Workers
   |                           |
   | sem_wait(empty)           |
   |                           |
   |---- add request --------->|
   |                           |
   | sem_post(full)            |
   |                           |
   |                           | sem_wait(full)
   |                           |
   |                           |---- remove request
   |                           |
   |                           | sem_post(empty)
```

---

## Worker Thread Recovery

The server contains a thread-monitoring function called `threadControl()`.

The main thread periodically checks each worker thread using:

```c
pthread_kill(workers[i], 0);
```

If a worker is detected as no longer existing, the server attempts to create a replacement worker thread.

The monitoring loop checks the workers approximately once per second.

```text
Main / Monitor
      |
      v
Check Worker 0 ---- alive
      |
Check Worker 1 ---- alive
      |
Check Worker 2 ---- dead
      |
      v
Create replacement Worker 2
      |
      v
Continue monitoring
```

---

## Client Architecture

The client uses POSIX threads to generate concurrent HTTP requests.

For each client thread, the program:

1. Creates a TCP socket.
2. Resolves the server hostname.
3. Establishes a TCP connection.
4. Builds an HTTP/1.0 GET request.
5. Sends the request.
6. Reads the server response.
7. Measures the request duration.
8. Reports the number of bytes received.
9. Closes the connection.

The HTTP request is constructed in the following form:

```text
GET /<page> HTTP/1.0
Host: <host>
User-Agent: HTMLGET 1.0
```

## The implementation sends the request and then continues reading until the server closes the connection.

## Performance Measurement

The client uses `gettimeofday()` to measure request and overall execution time.

Each client thread reports:

```text
[tid <thread-id>] received <bytes> bytes (<seconds>.<microseconds> sec).
```

After all client threads finish, the main thread prints:

```text
Time to handle <number> requests (<number> failed): <time> sec
```

This provides a simple way to observe how the server handles different numbers of concurrent requests.

---

## Example

### 1. Build

```bash
make
```

### 2. Start the server

```bash
./webserver_multi 8080 5
```

Example output:

```text
Created worker[0]
Created worker[1]
Created worker[2]
Created worker[3]
Created worker[4]
Created listener thread
HTTP server listening on port 8080
```

### 3. Run the client

In another terminal:

```bash
./client 127.0.0.1 8080 10
```

The client creates 10 concurrent requests.

Example output format:

```text
Request: GET 127.0.0.1:8080//, # of client: 10
[tid ...] received ... bytes (... sec).
[tid ...] received ... bytes (... sec).
...
Time to handle 10 requests (0 failed): ... sec
```

---

## Concurrency Model

The project demonstrates several important systems-programming concepts:

* TCP/IP socket programming
* Client-server architecture
* POSIX threads
* Thread synchronization
* Mutexes
* Semaphores
* Producer-consumer design
* Circular buffers
* Concurrent request processing
* Thread monitoring and recovery
* HTTP request construction
* Network performance measurement
* Dynamic memory management

The server separates **connection acceptance** from **request processing**, allowing the listener to continue accepting connections while worker threads handle previously accepted requests.

---

## Configuration Limits

### Server

```text
Request buffer:       100 connections
Maximum workers:      100
Listening port:       2001–49999
```

### Client

```text
Default client threads: 10
Maximum client threads: 100
Default page:          /
```

## These limits are defined directly in the source code and argument validation.

## Cleaning the Build

To remove the compiled executables:

```bash
make clean
```

This removes:

```text
webserver
webserver_multi
client
```

---

## Notes

The server implementation relies on the `process()` function and other networking functionality provided by the project's `webserver.c`, `net.c`, and `webserver.h` files. These files are included as build dependencies in the Makefile but were not part of the uploaded source files used to create this README.

The multi-threaded server is designed to run continuously. Its listener and worker functions contain infinite loops, while `threadControl()` continuously monitors the worker threads.
