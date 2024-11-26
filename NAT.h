#ifndef NAT_H
#define NAT_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <thread>
#include <netinet/in.h>

class NAT {
public:
    NAT(int port) : port(port) {}

    void init(); // Function declaration
    void add_mapping(const std::string& private_ip, const std::string& public_ip); // Function declaration
    std::string lookup(const std::string& private_ip); // Function declaration
    void handle_client(int client_socket); // Function declaration
    void start(); // Function declaration
    ~NAT(); // Destructor declaration

private:
    int port;
    int server_socket;
    struct sockaddr_in nat_address;
    std::unordered_map<std::string, std::string> nat_table; // Mapping of private IPs to public IPs
};

#endif
