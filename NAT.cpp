#include "NAT.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <ctime> // For timestamps

// Constructor
NAT::NAT(int port, int threshold) : port(port), threshold(threshold) {
    initialize_ip_pool();
}

// Initialize the pool of public IPs
void NAT::initialize_ip_pool() {
    for (int i = 1; i <= 10; ++i) {
        ip_pool.insert("203.0.113." + std::to_string(i));
    }
}

// Initialize the NAT server
void NAT::init() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

// Start the NAT server
void NAT::start() {
    // Start the cleanup thread
    std::thread([this]() { cleanup_mappings(); }).detach();

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

// Handle a single client connection
void NAT::handle_client(int client_socket) {
    char buffer[1024] = {0};
    struct timeval timeout = {20, 0};  // 20-second timeout
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (true) {
        int bytes_read = read(client_socket, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            std::string private_ip(buffer, bytes_read);
            std::time_t now = std::time(nullptr);
            std::cout << "Received data from client: " << private_ip << " at " << std::ctime(&now);

            {
                std::lock_guard<std::mutex> lock(nat_mutex);
                if (nat_table.find(private_ip) == nat_table.end()) {
                    add_mapping(private_ip);
                } else {
                    nat_table[private_ip].timestamp = now;
                    std::cout << "Updated timestamp for: " << private_ip << std::endl;
                }
            }
        } else if (bytes_read == 0 || errno == EWOULDBLOCK || errno == EAGAIN) {
            std::cout << "Client disconnected or timed out." << std::endl;
            break;
        } else {
            perror("Error reading from client");
            break;
        }
    }

    close(client_socket);
}

// Add a new mapping
void NAT::add_mapping(const std::string& private_ip) {
    if (!ip_pool.empty()) {
        std::string public_ip = *ip_pool.begin();
        ip_pool.erase(public_ip);

        nat_table[private_ip] = {public_ip, std::time(nullptr)};
        std::cout << "Mapping added: " << private_ip << " -> " << public_ip << std::endl;
    } else {
        std::cerr << "No available public IPs for mapping!" << std::endl;
    }
}

// Periodically clean up stale mappings
void NAT::cleanup_mappings() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::lock_guard<std::mutex> lock(nat_mutex);
        auto now = std::time(nullptr);

        for (auto it = nat_table.begin(); it != nat_table.end();) {
            if (now - it->second.timestamp > threshold) {
                std::cout << "Removing stale mapping: " << it->first << " -> " << it->second.public_ip << std::endl;
                release_ip(it->second.public_ip);
                it = nat_table.erase(it);
            } else {
                ++it;
            }
        }

        print_mappings();
    }
}

// Release a public IP back to the pool
void NAT::release_ip(const std::string& public_ip) {
    ip_pool.insert(public_ip);
}

// Print the current mappings
void NAT::print_mappings() {
    std::cout << "\nCurrent NAT Mappings:\n";
    for (const auto& entry : nat_table) {
        std::cout << entry.first << " -> " << entry.second.public_ip << " | Last updated: " << entry.second.timestamp << "\n";
    }
}

// Destructor
NAT::~NAT() {
    close(server_socket);
}
