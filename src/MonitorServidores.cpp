#include "MonitorServidores.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <system_error>
#include <csignal>
#include <unistd.h>  // Necesario para usar access()

std::atomic<bool> server1_active(true);
std::atomic<bool> server2_active(true);
std::atomic<bool> server3_active(true);

void start_server(int server_id, int port, std::atomic<bool>& server_active) {
    std::string command = "./build/chat servidor " + std::to_string(port);

    // Verificar si el archivo existe antes de ejecutar el comando
    if (access("./build/chat", F_OK) == -1) {
        std::cerr << "El archivo ./build/chat no existe o no es accesible." << std::endl;
        return;
    }

    while (true) {
        if (server_active) {
            std::cout << "Iniciando Servidor " << server_id << " en puerto " << port << std::endl;
            int result = std::system(command.c_str());
            if (result != 0) {
                std::cerr << "Servidor " << server_id << " se ha detenido con error " << result << " (código de salida: " << WEXITSTATUS(result) << ")" << std::endl;
                server_active = false;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void monitor_servers() {
    while (true) {
        if (!server1_active) {
            std::cout << "Reiniciando Servidor 1..." << std::endl;
            server1_active = true;
        }
        if (!server2_active) {
            std::cout << "Reiniciando Servidor 2..." << std::endl;
            server2_active = true;
        }
        if (!server3_active) {
            std::cout << "Reiniciando Servidor 3..." << std::endl;
            server3_active = true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Monitoreo cada 5 segundos
    }
}

int main() {
    std::vector<std::thread> server_threads;

    // Iniciar servidores en diferentes puertos
    server_threads.emplace_back(start_server, 1, 12345, std::ref(server1_active));
    server_threads.emplace_back(start_server, 2, 12346, std::ref(server2_active));
    server_threads.emplace_back(start_server, 3, 12347, std::ref(server3_active));

    // Iniciar monitoreo
    std::thread monitor_thread(monitor_servers);

    // Esperar a que los hilos terminen
    for (auto& t : server_threads) {
        t.join();
    }
    monitor_thread.join();

    return 0;
}

