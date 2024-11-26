#include "NAT.h"
#include <unistd.h>
#include <fstream>
#include <thread>
#include <ctime>
#include <chrono>

// Constructor
NAT::NAT(int port, int threshold) : port(port), threshold(threshold) {
    initialize_ip_pool();
}

// Load public IPs from file
void NAT::initialize_ip_pool() {
    std::ifstream pool_file("ip_pool.txt");
    std::string ip;
    if (pool_file.is_open()) {
        while (std::getline(pool_file, ip)) {
            ip_pool.insert(ip);
        }
        pool_file.close();
    } else {
        std::cerr << "Error: Unable to open IP pool file." << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Save updated pool back to file
void NAT::save_ip_pool() {
    std::ofstream pool_file("ip_pool.txt", std::ios::trunc);
    for (const auto& ip : ip_pool) {
        pool_file << ip << std::endl;
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

// Handle a single client
void NAT::handle_client(int client_socket) {
    char buffer[1024] = {0};
    int bytes_read = read(client_socket, buffer, sizeof(buffer));

    if (bytes_read > 0) {
        std::string private_ip(buffer, bytes_read);
        std::lock_guard<std::mutex> lock(nat_mutex);

        if (nat_table.find(private_ip) == nat_table.end()) {
            // Assign a public IP if mapping doesn't exist
            if (!ip_pool.empty()) {
                std::string public_ip = *ip_pool.begin();
                ip_pool.erase(public_ip);
                save_ip_pool();
                nat_table[private_ip] = {public_ip, std::time(nullptr)};
                std::cout << "Assigned: " << private_ip << " -> " << public_ip << std::endl;
            } else {
                std::cerr << "No available IPs in pool!" << std::endl;
            }
        } else {
            // Update timestamp if mapping exists
            nat_table[private_ip].timestamp = std::time(nullptr);
            std::cout << "Updated timestamp for: " << private_ip << std::endl;
        }

        std::string public_ip = nat_table[private_ip].public_ip;
        std::string response = private_ip + " -> " + public_ip;
        send(client_socket, response.c_str(), response.size(), 0);
    }

    close(client_socket);
}

// Start the NAT server
void NAT::start() {
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

// Periodically clean up stale mappings
void NAT::cleanup_mappings() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Cleanup interval

        std::lock_guard<std::mutex> lock(nat_mutex);
        auto now = std::time(nullptr);

        for (auto it = nat_table.begin(); it != nat_table.end();) {
            if (now - it->second.timestamp > threshold) {
                std::cout << "Removing stale mapping: " << it->first << " -> " << it->second.public_ip << std::endl;
                ip_pool.insert(it->second.public_ip);
                save_ip_pool();
                it = nat_table.erase(it);
            } else {
                ++it;
            }
        }

        print_mappings();
    }
}


void NAT::print_mappings() {
    std::cout << "\n=== Current NAT Mappings ===\n";
    std::cout << "| Private IP       | Public IP        | Last Updated           |\n";
    std::cout << "|------------------|------------------|-------------------------|\n";

    if (nat_table.empty()) {
        std::cout << "| No active mappings                                    |\n";
    } else {
        for (const auto& entry : nat_table) {
            std::time_t timestamp = entry.second.timestamp;
            std::cout << "| " << std::left << std::setw(16) << entry.first << " | "
                      << std::setw(16) << entry.second.public_ip << " | "
                      << std::setw(23) << std::ctime(&timestamp); // Includes newline
        }
    }

    std::cout << "=========================================\n";
}



// Destructor
NAT::~NAT() {
    close(server_socket);
}
