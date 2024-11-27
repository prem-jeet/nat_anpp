#include "NAT.h"
#include <unistd.h>
#include <fstream>
#include <thread>
#include <ctime>
#include <chrono>
#include <sys/stat.h>
#include <iomanip>

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
    log_event("Starting NAT server initialization.");
    load_mappings_from_file();
    initialize_ip_pool();

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        log_event("Socket creation failed. Exiting.");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    nat_address.sin_family = AF_INET;
    nat_address.sin_addr.s_addr = INADDR_ANY;
    nat_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&nat_address, sizeof(nat_address)) < 0) {
        perror("Bind failed");
        log_event("Bind failed. Exiting.");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        log_event("Listen failed. Exiting.");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    std::cout << "NAT server listening on port " << port << std::endl;
    log_event("NAT server initialized and listening on port " + std::to_string(port));
}

// Monitor the configuration file for changes
void NAT::watch_config_file() {
    std::string conf_file = "nat_conf.txt";
    std::time_t last_modified_time = 0;

    while (true) {
        struct stat file_stat;
        if (stat(conf_file.c_str(), &file_stat) == 0) {
            if (file_stat.st_mtime != last_modified_time) {
                last_modified_time = file_stat.st_mtime;
                apply_config_changes(conf_file);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Check every 5 seconds
    }
}

// Apply configuration file changes
void NAT::apply_config_changes(const std::string& conf_file) {
    std::ifstream file(conf_file);
    if (!file.is_open()) {
        log_event("Failed to open config file: " + conf_file);
        return;
    }

    std::unordered_map<std::string, std::string> new_mappings;
    std::string private_ip, public_ip;

    // Read all mappings from the config file
    while (file >> private_ip >> public_ip) {
        new_mappings[private_ip] = public_ip;
    }

    file.close();

    // Update mappings in memory and ensure file is consistent
    std::lock_guard<std::mutex> lock(nat_mutex);
    for (const auto& entry : new_mappings) {
        const std::string& private_ip = entry.first;
        const std::string& public_ip = entry.second;

        if (nat_table.find(private_ip) == nat_table.end() || nat_table[private_ip].public_ip != public_ip) {
            // Update the mapping
            nat_table[private_ip] = {public_ip, std::time(nullptr)};
            log_event("Config update: " + private_ip + " -> " + public_ip);
        }
    }

    // Save all mappings to file
    save_mapping_to_file("", {});
}


// Handle a single client
void NAT::handle_client(int client_socket) {
    char buffer[1024] = {0};
    int bytes_read = read(client_socket, buffer, sizeof(buffer));

    if (bytes_read > 0) {
        std::string private_ip(buffer, bytes_read);
        log_event("Client connected with private IP: " + private_ip);

        std::lock_guard<std::mutex> lock(nat_mutex);

        // Step 1: Check `nat_conf.txt` for a predefined mapping
        bool found_in_config = false;
        std::ifstream config_file("nat_conf.txt");
        std::string conf_private_ip, conf_public_ip;

        if (config_file.is_open()) {
            while (config_file >> conf_private_ip >> conf_public_ip) {
                if (conf_private_ip == private_ip) {
                    nat_table[private_ip] = {conf_public_ip, std::time(nullptr)};
                    save_mapping_to_file(private_ip, nat_table[private_ip]);
                    found_in_config = true;
                    log_event("Assigned from config: " + private_ip + " -> " + conf_public_ip);
                    std::string response = "Assigned from config: " + private_ip + " -> " + conf_public_ip;
                    send(client_socket, response.c_str(), response.length(), 0);
                    break;
                }
            }
            config_file.close();
        }

        if (!found_in_config) {
            // Step 2: Check existing mappings in `nat_table`
            if (nat_table.find(private_ip) == nat_table.end()) {
                // Step 3: Assign a new mapping if none exists
                if (!ip_pool.empty()) {
                    std::string public_ip = *ip_pool.begin();
                    ip_pool.erase(public_ip);
                    save_ip_pool();

                    nat_table[private_ip] = {public_ip, std::time(nullptr)};
                    save_mapping_to_file(private_ip, nat_table[private_ip]);

                    log_event("New mapping created: " + private_ip + " -> " + public_ip);
                    std::string response = "New mapping: " + private_ip + " -> " + public_ip;
                    send(client_socket, response.c_str(), response.length(), 0);
                } else {
                    std::string response = "No available IPs in pool!";
                    send(client_socket, response.c_str(), response.length(), 0);
                    log_event("Failed to assign IP for " + private_ip + ": " + response);
                }
            } else {
                // Existing mapping found
                std::string public_ip = nat_table[private_ip].public_ip;
                nat_table[private_ip].timestamp = std::time(nullptr);
                save_mapping_to_file(private_ip, nat_table[private_ip]);

                log_event("Existing mapping: " + private_ip + " -> " + public_ip);
                std::string response = "Existing mapping: " + private_ip + " -> " + public_ip;
                send(client_socket, response.c_str(), response.length(), 0);
            }
        }
    } else {
        log_event("Client disconnected unexpectedly.");
        std::string response = "Error: Client disconnected unexpectedly.";
        send(client_socket, response.c_str(), response.length(), 0);
    }

    close(client_socket);
    log_event("Client connection closed.");
}



// Start the NAT server
void NAT::start() {
    std::thread([this]() { cleanup_mappings(); }).detach();
    std::thread([this]() { watch_config_file(); }).detach();

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
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::lock_guard<std::mutex> lock(nat_mutex);
        auto now = std::time(nullptr);

        bool file_updated = false;

        for (auto it = nat_table.begin(); it != nat_table.end();) {
            if (now - it->second.timestamp > threshold) {
                log_event("Removing stale mapping: " + it->first + " -> " + it->second.public_ip);

                ip_pool.insert(it->second.public_ip);
                save_ip_pool();

                it = nat_table.erase(it);
                file_updated = true;
            } else {
                ++it;
            }
        }

        if (file_updated) {
            save_mapping_to_file("", {}); // Rewrite the mapping file with updated entries
        }

        print_mappings();
    }
}



// Load existing mappings from file
void NAT::load_mappings_from_file() {
    std::ifstream mapping_file("nat_mappings.txt");
    if (!mapping_file.is_open()) {
        std::cerr << "No existing mapping file found. Starting fresh." << std::endl;
        return;
    }

    std::string private_ip, public_ip;
    std::time_t timestamp;
    while (mapping_file >> private_ip >> public_ip >> timestamp) {
        nat_table[private_ip] = {public_ip, timestamp};
    }

    mapping_file.close();
}

// Save all mappings to the file
void NAT::save_mapping_to_file(const std::string&, const Mapping&) {
    std::ofstream mapping_file("nat_mappings.txt", std::ios::trunc);
    if (!mapping_file.is_open()) {
        std::cerr << "Error: Unable to open mapping file for writing." << std::endl;
        return;
    }

    for (const auto& entry : nat_table) {
        mapping_file << entry.first << " " << entry.second.public_ip << " " << entry.second.timestamp << "\n";
    }

    mapping_file.close();
}


// Print all mappings
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

// Log events to the log file
void NAT::log_event(const std::string& event) {
    std::ofstream log_file("nat_server.log", std::ios::app); // Open in append mode
    if (log_file.is_open()) {
        std::time_t now = std::time(nullptr);
        log_file << std::ctime(&now) << " - " << event << std::endl;
        log_file.close();
    } else {
        std::cerr << "Error: Unable to write to log file." << std::endl;
    }
}

// Destructor
NAT::~NAT() {
    close(server_socket);
}
