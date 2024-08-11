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
#include <cstring>
#include <sstream>

std::vector<std::shared_ptr<std::atomic<bool>>> server_active;

// Comprueba si el puerto esta disponible
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


// Inicia un servidor en el puerto especificado
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

// Monitorea los servidores y los reinicia si es necesario
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


// Recibe mensajes de los servidores
void recibirInformacionServidor() {
    int descriptorMonitor = socket(AF_INET, SOCK_DGRAM, 0);
    if (descriptorMonitor == -1) {
        std::cerr << "Error al crear el socket para recibir.\n";
        return;
    }

    sockaddr_in direccionMonitor;
    direccionMonitor.sin_family = AF_INET;
    direccionMonitor.sin_port = htons(55555); // Puerto para recibir los datos
    direccionMonitor.sin_addr.s_addr = INADDR_ANY;

    if (bind(descriptorMonitor, (sockaddr*)&direccionMonitor, sizeof(direccionMonitor)) == -1) {
        std::cerr << "Error al hacer bind del socket del monitor.\n";
        return;
    }

    char buffer[1024];
    sockaddr_in emisorDireccion;
    socklen_t emisorTamano = sizeof(emisorDireccion);

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRecibidos = recvfrom(descriptorMonitor, buffer, 1024, 0, (sockaddr*)&emisorDireccion, &emisorTamano);
        if (bytesRecibidos > 0) {
            // Null-terminar el buffer recibido
            buffer[bytesRecibidos] = '\0';

            // Convertir el buffer a std::string
            std::string received_data(buffer);

            // Separar el string en un array de strings
            std::vector<std::string> messages;
            std::istringstream stream(received_data);
            std::string token;
            while (std::getline(stream, token, '\n')) {  // Usa el delimitador '\n'
                if (!token.empty()) {
                    messages.push_back(token);
                }
            }

            // Imprimir los mensajes recibidos
            for (const auto& msg : messages) {
                std::cout << "Mensaje recibido: " << msg << std::endl;
            }
        }
    }

    close(descriptorMonitor);
}


//Función principal
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

    std::thread recibirHilo(recibirInformacionServidor);
    recibirHilo.detach();

    // Esperar a que los hilos terminen
    for (auto& t : server_threads) {
        t.join();
    }
    monitor_thread.join();

    return 0;
}



