#include "NAT.h"

/* Compile with:
 * g++ NAT.cpp main.cpp -o nat_server -pthread
 */

int main() {
    // Define NAT server parameters
    int port = 55000;        // NAT server port
    int threshold = 30;      // Cleanup threshold in seconds

    // Initialize the NAT server
    NAT nat_server(port, threshold);
    nat_server.init();

    std::cout << "NAT server initialized and ready to handle dynamic mappings.\n";

    // Start the NAT server
    nat_server.start();

    return 0;
}
