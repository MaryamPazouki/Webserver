// =====================================================================================
//                               Simple Poll-Based Webserver
// =====================================================================================
//
// This file implements a **minimal working HTTP server** using:
//
//   - socket()   : create a listening socket
//   - bind()     : attach it to a port (8080)
//   - listen()   : start listening for connections
//   - accept()   : accept new clients
//   - poll()     : handle multiple clients without blocking
//   - read()     : receive their HTTP request
//   - write()    : send back an HTTP response
//   - close()    : close sockets cleanly
//
// =====================================================================================


#include <iostream>         // For std::cout, std::cerr
#include <cstring>          // For std::memset
#include <vector>           // For std::vector (dynamic pollfd list)
#include <netinet/in.h>     // For sockaddr_in, htons(), INADDR_ANY
#include <sys/socket.h>     // For socket(), bind(), listen(), accept(), recv(), send()
#include <unistd.h>         // For close()
#include <fcntl.h>          // For fcntl(), O_NONBLOCK
#include <poll.h>           // For poll(), pollfd structure



//--------------------------------------------------------------
// Build a VERY basic HTTP response (just to test)
//--------------------------------------------------------------
std::string buildResponse()
{
    return
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 12\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello world!";
}


int main()
{
    // =================================================================================
    // 1. CREATE A NON-BLOCKING SERVER SOCKET
    // =================================================================================
    //
    // socket():
    //   AF_INET      → IPv4
    //   SOCK_STREAM  → TCP (connection-based, reliable)
    //   0            → default TCP protocol
    //
    // Returns:
    //   ≥ 0 → valid socket descriptor
    //   -1  → failure
    // =================================================================================
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd < 0)
    {
        std::cerr << "Error: socket() failed\n";
        return 1;
    }



    /* int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0)
    {
        std::cerr << "Error: socket() failed\n";
        return 1;
    } */

   

    // =================================================================================
    // 2. PREPARE THE SERVER ADDRESS (IPv4 + port 8080)
    // =================================================================================
    sockaddr_in serverAddress;

    // Zero the memory (important!)
    std::memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;       // IPv4
    serverAddress.sin_port   = htons(8080);   // Port 8080 (network order)
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    // INADDR_ANY = bind on ALL available interfaces
    // Example: 127.0.0.1, 192.168.x.x, etc.

    // =================================================================================
    // 3. BIND THE SOCKET TO THE ADDRESS
    // =================================================================================
    if (bind(listen_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        std::cerr << "Error: bind() failed\n";
        close(listen_fd);
        return 1;
    }

    // =================================================================================
    // 4. LISTEN FOR INCOMING CONNECTIONS
    // =================================================================================
    //
    // listen():
    //   - Converts a normal socket → "listening" socket
    //   - The number 10 is the backlog (queue size for pending clients)
    // =================================================================================
    if (listen(listen_fd, 10) < 0)
    {
        std::cerr << "Error: listen() failed\n";
        close(listen_fd);
        return 1;
    }
     // ---------------------------------------------------------------------------------
    // Make the server socket NON-BLOCKING
    // ---------------------------------------------------------------------------------
    //
    // O_NONBLOCK means:
    //   - accept() NEVER blocks
    //   - recv()   NEVER blocks
    //   - send()   NEVER blocks
    //
    // This is essential for an event-driven server. The entire server runs inside ONE
    // thread, so we MUST ensure no call ever freezes the whole program.
    // ---------------------------------------------------------------------------------
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);
    std::cout << "Non-blocking server running on port 8080...\n";

    // =================================================================================
    // 5. PREPARE POLL ARRAY (VECTOR OF pollfd)
    // =================================================================================
    //
    // pollfd contains:
    //   fd      → socket descriptor
    //   events  → what we WANT to monitor
    //   revents → what ACTUALLY happened (set by poll)
    //
    // sockets[0] = server socket (used to accept new clients)
    // sockets[1..] = connected clients
    // =================================================================================
    std::vector<pollfd> fds;

    pollfd listenPollEntry;
    listenPollEntry.fd = listen_fd;
    listenPollEntry.events = POLLIN;    // Notify when new client attempts to connect
    listenPollEntry.revents = 0;

    fds.push_back(listenPollEntry);

    // =================================================================================
    // 6. MAIN EVENT LOOP (POLL-BASED)
    // =================================================================================
    //
    // poll():
    //   - Blocks until any socket becomes active
    //   - Or until timeout expires
    //
    // Behavior:
    //   fds[i].revents & POLLIN  → socket ready to read
    //   fds[i].revents & POLLOUT → socket ready to write
    // =================================================================================
    while (true)
    {
        int activity = poll(fds.data(), fds.size(), 500);  // 500 ms timeout

        if (activity < 0)
        {
            std::cerr << "Error: poll() failed\n";
            break;
        }

//------------------------------------------------------
        // 6. Check if the listening socket has a new connection
        //------------------------------------------------------
        if (fds[0].revents & POLLIN)
        {
            // Accept as many connections as available (non-blocking)
            while (true)
            {
                int client_fd = accept(listen_fd, NULL, NULL);

                if (client_fd < 0)
                    break; // No more connections to accept

                std::cout << "New client connected: " << client_fd << "\n";

                // Make client non-blocking
                fcntl(client_fd, F_SETFL, O_NONBLOCK);

                // Add new client to poll list
                pollfd client_pollfd;
                client_pollfd.fd      = client_fd;
                client_pollfd.events  = POLLIN;   // Wait for client to send data
                client_pollfd.revents = 0;

                fds.push_back(client_pollfd);
            }
        }

        //------------------------------------------------------
        // 7. Handle events from EXISTING clients
        //------------------------------------------------------
        for (size_t i = 1; i < fds.size(); i++)
        {
            pollfd &pfd = fds[i];

            //--------------------------------------------------
            // If client sent data (POLLIN)
            //--------------------------------------------------
            if (pfd.revents & POLLIN)
            {
                char buffer[1024];
                ssize_t bytes = recv(pfd.fd, buffer, sizeof(buffer), 0);

                if (bytes <= 0)
                {
                    // Client disconnected or error
                    std::cout << "Client disconnected: " << pfd.fd << "\n";
                    close(pfd.fd);

                    // Remove from poll list
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }

                // We received a request!
                std::string req(buffer, bytes);
                std::cout << "Received request from " << pfd.fd << ":\n";
                std::cout << req << "\n";

                //--------------------------------------------------
                // Send response
                //--------------------------------------------------
                std::string response = buildResponse();
                send(pfd.fd, response.c_str(), response.size(), 0);

                //--------------------------------------------------
                // Close connection after response (simple HTTP/1.1)
                //--------------------------------------------------
                std::cout << "Closing client " << pfd.fd << "\n";
                close(pfd.fd);
                fds.erase(fds.begin() + i);
                i--;
            }
        }
    }
    close(listen_fd);
    return 0;
}
