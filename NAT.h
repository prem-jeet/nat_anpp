#ifndef NAT_H
#define NAT_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <mutex>
#include <set>
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
};

#endif
