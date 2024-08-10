#include "MonitorServidores.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <system_error>
#include <csignal>
#include <unistd.h>  // Necesario para usar access()
// Incluir las cabeceras necesarias para sockets
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory>  // Para std::shared_ptr

std::vector<std::shared_ptr<std::atomic<bool>>> server_active;

bool is_port_available(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error al crear un socket temporal para verificar el puerto.\n";
        return false;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    bool available = (bind(sockfd, (sockaddr*)&address, sizeof(address)) == 0);
    close(sockfd);
    return available;
}

void start_server(int server_id, int port, std::shared_ptr<std::atomic<bool>> server_active) {
    std::string command = "./build/chat servidor " + std::to_string(port);

    // Verificar si el archivo existe antes de ejecutar el comando
    if (access("./build/chat", F_OK) == -1) {
        std::cerr << "El archivo ./build/chat no existe o no es accesible." << std::endl;
        return;
    }

    while (true) {
        if (*server_active) {
            if (is_port_available(port)) {
                std::cout << "Iniciando Servidor " << server_id << " en puerto " << port << std::endl;
                int result = std::system(command.c_str());
                if (result != 0) {
                    std::cerr << "Servidor " << server_id << " se ha detenido con error " << result << " (código de salida: " << WEXITSTATUS(result) << ")" << std::endl;
                    *server_active = false;

                    // Intentar de nuevo después de un breve retraso
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            } else {
                std::cerr << "El puerto " << port << " no está disponible. Intentando nuevamente en 5 segundos...\n";
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        } else {
            // Esperar un poco antes de volver a verificar si se debe reiniciar
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void monitor_servers() {
    while (true) {
        for (size_t i = 0; i < server_active.size(); ++i) {
            if (!*server_active[i]) {
                std::cout << "Reiniciando Servidor " << i + 1 << "...\n";
                *server_active[i] = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Monitoreo cada 5 segundos
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3 || (argc - 1) % 2 != 0) {
        std::cerr << "Uso: " << argv[0] << " <num_servidores> <puerto1> ... <puertoN>\n";
        return 1;
    }

    int num_servers = std::stoi(argv[1]);
    if (num_servers <= 0 || argc != 2 + num_servers) {
        std::cerr << "Número de servidores inválido o número incorrecto de puertos.\n";
        return 1;
    }

    std::vector<int> ports;
    for (int i = 2; i < argc; ++i) {
        ports.push_back(std::stoi(argv[i]));
    }

    server_active.resize(num_servers);

    // Inicializar el vector de std::shared_ptr<std::atomic<bool>>
    for (int i = 0; i < num_servers; ++i) {
        server_active[i] = std::make_shared<std::atomic<bool>>(true);
    }

    std::vector<std::thread> server_threads;

    // Iniciar servidores en los puertos especificados usando lambda functions
    for (int i = 0; i < num_servers; ++i) {
        server_threads.emplace_back([i, &ports]() {
            start_server(i + 1, ports[i], server_active[i]);
        });
    }

    // Iniciar monitoreo
    std::thread monitor_thread(monitor_servers);

    // Esperar a que los hilos terminen
    for (auto& t : server_threads) {
        t.join();
    }
    monitor_thread.join();

    return 0;
}

