#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 55000

void run_client(const std::string& server_ip, int port, const std::string& private_ip) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection to NAT server failed");
        close(sock);
        return;
    }

    send(sock, private_ip.c_str(), private_ip.length(), 0);
    char buffer[1024] = {0};
    read(sock, buffer, 1024);

    std::cout << "Response from NAT: " << buffer << std::endl;

    close(sock);
}

int main(int argc, char* argv[]) {
    std::string server_ip = DEFAULT_SERVER_IP;
    int port = DEFAULT_SERVER_PORT;
    std::string private_ip;

    if (argc == 2) {
        private_ip = argv[1];
    } else if (argc == 4) {
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
