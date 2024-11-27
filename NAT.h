#ifndef NAT_H
#define NAT_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <mutex>
#include <set>
#include <netinet/in.h>
#include <fstream>
#include <iomanip>
#include <sys/stat.h>

struct Mapping {
    std::string public_ip;
    std::time_t timestamp;
};

class NAT {
public:
    NAT(int port, int threshold);
    void init();
    void start();
    ~NAT();

private:
    int port;
    int threshold;
    int server_socket;
    struct sockaddr_in nat_address;
    std::unordered_map<std::string, Mapping> nat_table;
    std::set<std::string> ip_pool;
    std::mutex nat_mutex;

    // Core methods
    void initialize_ip_pool();
    void save_ip_pool();
    void handle_client(int client_socket);
    void cleanup_mappings();
    void print_mappings();
    void log_event(const std::string& event);

    // File-related methods
    void load_mappings_from_file();
    void save_mapping_to_file(const std::string& private_ip, const Mapping& mapping);

    // New methods for configuration updates
    void watch_config_file(); // Monitor configuration file
    void apply_config_changes(const std::string& conf_file); // Apply changes from file
};

#endif
