-------Non-Blocking Mode + poll()--------

We will:

-Understand blocking vs non-blocking sockets

-Understand what poll() does

-Build a fully commented mini server using poll()

-Prepare for multiple clients + HTTP parsing



--------Blocking (default)--------------

accept(), read(), recv(), send() will FREEZE your program if:

no client connects (accept blocks)

client sends no data (read blocks)

client buffer is full (send blocks)

That's OK for the small demo server…
But Webserv must handle MANY clients at the same time.

So blocking = impossible.

---------Non-blocking Mode--------------

We use:

        fcntl(socket_fd, F_SETFL, O_NONBLOCK);

After this, calls like accept() and recv() no longer freeze the program.

Instead, they return instantly with:

- -1

- and set errno to EWOULDBLOCK

That’s good:

it means “nothing to read right now, continue your event loop”.



-----------------3. poll() — the heart of Webserv

poll() tells us:

-which sockets are ready to read

-which sockets can write

-which sockets are closed

poll() signature (simplified):

          poll(struct pollfd *fds, nfds_t nfds, int timeout);


fds: array of sockets to watch
nfds: how many sockets
timeout: milliseconds (we use maybe 100 or 500ms)

Each entry:

struct pollfd {
    int   fd;        // socket
    short events;    // what we want to monitor (POLLIN, POLLOUT…)
    short revents;   // what ACTUALLY happened
};


---------- poll()-based mini-server

This server:

-Is NON-BLOCKING
-Uses poll() in an event loop
-Accepts multiple clients
-Prints when clients connect
-Prints when clients send data
-Echoes data back
-Never blocks the program

----------------------------------------
------  events and revents----------------
-----------------------------------------
-events is your request
-and revents is the kernel's response.

Think of it like ordering at a restaurant:

-events = what you ordered
-revents = what the waiter brings to your table


=>Common events flags:

Flag	Meaning
POLLIN	Ready to read (data available, or new client on listening socket)
POLLOUT	Ready to write (socket can send data without blocking)
POLLERR	Error happened
POLLHUP	Client disconnected
POLLNVAL	Invalid fd

=> revents tells you:

-A client sent data
-A new connection arrived
-Some socket is ready to write
-A socket closed
-An error occurred

This is how your server knows what to do next.




Important webserver behavior:

On the server socket (listen_fd):

-If revents & POLLIN → accept new connection

On client sockets:

-If revents & POLLIN → incoming HTTP request
-If revents & POLLOUT → ready to send HTTP response
-If revents & POLLHUP → client disconnected
-If revents & POLLERR → remove client

--Quick mental model---
Field	 Set       byPurpose
events	 YOU	    what you want to monitor
revents	 POLL	    What actually happened

---------------------------------------
------Poll-----------------------------
poll() is a traffic controller for all your sockets.

You tell poll():

"Here are all my sockets.
Tell me which ones need attention."

Then it blocks and wakes up only when something happens.

---------------------------------------------
-----------PULLIN/ PULLOUT---------------------
When to Use POLLIN and POLLOUT
🔽 POLLIN

We want to read data.

Use it for:
    -Listening socket (new client wants to join)
    -Client socket (client is sending a request)

Example:
    fds[i].events = POLLIN;

🔼 POLLOUT

We want to write data.

Use it for:
  -Sending HTTP response to client

Example:
    fds[i].events = POLLOUT;

----------SUMMERY---------------------





| Concept          | Simple Explanation                            |
| ---------------- | --------------------------------------------- |
| `poll()`         | Wait until any socket needs attention         |
| event loop       | Runs forever, handles all client interactions |
| events           | What we *want* to detect                      |
| revents          | What *actually happened*                      |
| POLLIN           | Ready to read (new client or request data)    |
| POLLOUT          | Ready to write (send response)                |
| listening socket | Only for accepting new clients                |
| client sockets   | Need to be monitored for reading/writing      |
| closing sockets  | Remove from poll list to avoid errors         |
