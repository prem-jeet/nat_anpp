#include "NAT.h"
/*g++ NAT.cpp main.cpp -o nat_server -pthread
*/
int main() {
    // Define NAT server on port 55000
    int port = 55000;
    NAT nat_server(port);

    // Initialize the NAT server
    nat_server.init();

    // Add some static mappings (Private IP -> Public IP)
    nat_server.add_mapping("192.168.1.1", "203.0.113.1");
    nat_server.add_mapping("192.168.1.2", "203.0.113.2");
    nat_server.add_mapping("192.168.1.3", "203.0.113.3");

    std::cout << "NAT server initialized with mappings:\n";
    std::cout << "192.168.1.1 -> 203.0.113.1\n";
    std::cout << "192.168.1.2 -> 203.0.113.2\n";
    std::cout << "192.168.1.3 -> 203.0.113.3\n";

    // Start the NAT server
    nat_server.start();

    return 0;
}
