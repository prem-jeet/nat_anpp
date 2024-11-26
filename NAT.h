#ifndef NAT_H
#define NAT_H

#include <iostream>
#include <unordered_map>
#include <set>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <netinet/in.h>

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
    int threshold;  // Threshold in seconds for stale mappings
    int server_socket;
    struct sockaddr_in nat_address;
    std::unordered_map<std::string, Mapping> nat_table;
    std::set<std::string> ip_pool;
    std::mutex nat_mutex;

    void initialize_ip_pool();
    void add_mapping(const std::string& private_ip);
    void cleanup_mappings();
    void handle_client(int client_socket);
    void release_ip(const std::string& public_ip);
    void print_mappings();
};

#endif
