#include "NAT.h"
#include <unistd.h>  // For close() and read()
#include <iostream>
#include <unordered_map>
#include <string>
#include <thread>
#include <netinet/in.h>

// Initialize the NAT server socket
void NAT::init() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    nat_address.sin_family = AF_INET;
    nat_address.sin_addr.s_addr = INADDR_ANY;
    nat_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&nat_address, sizeof(nat_address)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    std::cout << "NAT server listening on port " << port << std::endl;
}

// Add a private-to-public IP mapping
void NAT::add_mapping(const std::string& private_ip, const std::string& public_ip) {
    nat_table[private_ip] = public_ip;
}

// Lookup a private IP in the NAT table
std::string NAT::lookup(const std::string& private_ip) {
    if (nat_table.find(private_ip) != nat_table.end()) {
        return nat_table[private_ip];
    }
    return ""; // No mapping found
}

// Handle a single client connection
void NAT::handle_client(int client_socket) {
    char buffer[1024] = {0};
    int bytes_read = read(client_socket, buffer, sizeof(buffer));

    if (bytes_read > 0) {
        std::string private_ip(buffer);
        std::string public_ip = lookup(private_ip);

        if (public_ip.empty()) {
            std::string response = "No mapping for " + private_ip;
            send(client_socket, response.c_str(), response.size(), 0);
            std::cout << "No mapping found for: " << private_ip << std::endl;
        } else {
            std::string response = private_ip + " -> " + public_ip;
            send(client_socket, response.c_str(), response.size(), 0);
            std::cout << "Translated: " << private_ip << " -> " << public_ip << std::endl;
        }
    }

    close(client_socket);
}

// Start the NAT server to handle client requests
void NAT::start() {
    while (true) {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);

        if (client_socket >= 0) {
            std::cout << "New client connected" << std::endl;
            std::thread(&NAT::handle_client, this, client_socket).detach();
        }
    }
}

// Destructor to clean up the server socket
NAT::~NAT() {
    close(server_socket);
}
