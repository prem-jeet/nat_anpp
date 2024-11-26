#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <ctime> // For timestamps

#define DEFAULT_SERVER_IP "127.0.0.1" // Default server IP
#define DEFAULT_SERVER_PORT 55000     // Default server port
#define HEARTBEAT_INTERVAL 8          // Client sends heartbeats every 8 seconds
#define DISCONNECT_TIME 30            // Total runtime before client disconnects

void send_heartbeat(int sock, const std::string& private_ip) {
    std::cout << "Heartbeat function started for IP: " << private_ip << std::endl;

    // Send the first heartbeat immediately
    std::time_t now = std::time(nullptr);
    if (send(sock, private_ip.c_str(), private_ip.length(), 0) < 0) {
        perror("Error sending initial heartbeat");
        std::cerr << "Stopping heartbeat due to error." << std::endl;
        return; // Exit if the server is disconnected
    }
    std::cout << "Sent initial heartbeat: " << private_ip << " at " << std::ctime(&now);

    auto next_heartbeat = std::chrono::steady_clock::now() + std::chrono::seconds(HEARTBEAT_INTERVAL);

    // Continue sending periodic heartbeats
    while (true) {
        std::this_thread::sleep_until(next_heartbeat);
        next_heartbeat += std::chrono::seconds(HEARTBEAT_INTERVAL);

        now = std::time(nullptr);
        std::cout << "Attempting to send heartbeat at " << std::ctime(&now);

        if (send(sock, private_ip.c_str(), private_ip.length(), 0) < 0) {
            perror("Error sending heartbeat");
            std::cerr << "Stopping heartbeat due to error." << std::endl;
            return; // Exit if the server is disconnected
        }

        std::cout << "Sent heartbeat: " << private_ip << " at " << std::ctime(&now);
    }
}

void run_client(const std::string& server_ip, int port, const std::string& private_ip) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to NAT server failed");
        close(sock);
        return;
    }

    // Send initial mapping request
    if (send(sock, private_ip.c_str(), private_ip.length(), 0) < 0) {
        perror("Error sending initial mapping request");
        close(sock);
        return;
    }
    std::cout << "Initial mapping request sent: " << private_ip << std::endl;

    char buffer[1024] = {0};
    if (read(sock, buffer, sizeof(buffer)) > 0) {
        std::cout << "Response from NAT: " << buffer << std::endl;
    }

    // Start the heartbeat thread
    std::cout << "Starting heartbeat thread..." << std::endl;
    std::thread heartbeat_thread([sock, private_ip]() {
        send_heartbeat(sock, private_ip);
    });

    // Keep the main thread alive for DISCONNECT_TIME
    std::this_thread::sleep_for(std::chrono::seconds(DISCONNECT_TIME));

    std::cout << "Client disconnecting now." << std::endl;

    // Close the socket
    close(sock);

    // Wait for the heartbeat thread to finish
    if (heartbeat_thread.joinable()) {
        heartbeat_thread.join();
    }
}

int main(int argc, char* argv[]) {
    std::string server_ip = DEFAULT_SERVER_IP;
    int port = DEFAULT_SERVER_PORT;
    std::string private_ip;

    if (argc == 2) {
        // Only private IP is given
        private_ip = argv[1];
    } else if (argc == 4) {
        // Server IP, port, and private IP are given
        server_ip = argv[1];
        port = std::stoi(argv[2]);
        private_ip = argv[3];
    } else {
        std::cerr << "Usage: " << argv[0] << " <private_ip> OR <server_ip> <port> <private_ip>" << std::endl;
        return 1;
    }

    run_client(server_ip, port, private_ip);

    return 0;
}
