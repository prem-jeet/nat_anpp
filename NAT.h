#ifndef NAT_H
#define NAT_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <mutex>
#include <set>
#include <netinet/in.h>
#include <fstream>

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

    void initialize_ip_pool();
    void save_ip_pool();
    void handle_client(int client_socket);
    void cleanup_mappings();
    void print_mappings();

    // File-related functions
    void load_mappings_from_file();
    void save_mapping_to_file(const std::string& private_ip, const Mapping& mapping);
    void remove_mapping_from_file(const std::string& private_ip);
};

#endif
