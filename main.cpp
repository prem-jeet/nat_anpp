#include "NAT.h"

int main() {
    int port = 55000;
    int threshold = 20; // Threshold in seconds

    NAT nat_server(port, threshold);

    nat_server.init();
    nat_server.start();

    return 0;
}
