#ifndef MONITORSERVIDORES_H
#define MONITORSERVIDORES_H

#include <atomic>

void start_server(int server_id, int port, std::atomic<bool>& server_active);
void monitor_servers();

#endif // MONITORSERVIDORES_H