#include "MonitorServidores.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <system_error>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory>
#include <cstring>
#include <sstream>

// Vector para llevar el estado de los servidores (activos o no)
std::vector<std::shared_ptr<std::atomic<bool>>> server_active;

// Función para verificar si un puerto está disponible para su uso
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

    // Intenta hacer bind al puerto; si tiene éxito, el puerto está disponible
    bool available = (bind(sockfd, (sockaddr*)&address, sizeof(address)) == 0);
    close(sockfd); // Cierra el socket después de verificar
    return available;
}

// Función para iniciar un servidor en un puerto dado
void start_server(int server_id, int port, std::shared_ptr<std::atomic<bool>> server_active) {
    // Comando para ejecutar el servidor con el puerto especificado
    std::string command = "./build/chat servidor " + std::to_string(port);

    // Verifica si el archivo ejecutable existe y es accesible
    if (access("./build/chat", F_OK) == -1) {
        std::cerr << "El archivo ./build/chat no existe o no es accesible." << std::endl;
        return;
    }

    while (true) {
        if (*server_active) {
            if (is_port_available(port)) {
                std::cout << "Iniciando Servidor " << server_id << " en puerto " << port << std::endl;
                // Ejecuta el comando para iniciar el servidor
                int result = std::system(command.c_str());
                if (result != 0) {
                    std::cerr << "Servidor " << server_id << " se ha detenido con error " << result << " (código de salida: " << WEXITSTATUS(result) << ")" << std::endl;
                    // Marca el servidor como inactivo si hubo un error
                    *server_active = false;
                    std::this_thread::sleep_for(std::chrono::seconds(5)); // Espera antes de intentar reiniciar
                }
            } else {
                std::cerr << "El puerto " << port << " no está disponible. Intentando nuevamente en 5 segundos...\n";
                std::this_thread::sleep_for(std::chrono::seconds(5)); // Espera antes de volver a intentar
            }
        } else {
            std::this_thread::sleep_for(std::chrono::seconds(1)); // Espera si el servidor está inactivo
        }
    }
}

// Función para monitorizar y reiniciar servidores si es necesario
void monitor_servers() {
    while (true) {
        for (size_t i = 0; i < server_active.size(); ++i) {
            if (!*server_active[i]) {
                std::cout << "Reiniciando Servidor " << i + 1 << "...\n";
                // Marca el servidor como activo para intentar reiniciarlo
                *server_active[i] = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Espera antes de volver a comprobar el estado de los servidores
    }
}

// Función para recibir información de los servidores a través de un socket UDP
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

    // Asocia el socket al puerto para recibir datos
    if (bind(descriptorMonitor, (sockaddr*)&direccionMonitor, sizeof(direccionMonitor)) == -1) {
        std::cerr << "Error al hacer bind del socket del monitor.\n";
        return;
    }

    char buffer[1024];
    sockaddr_in emisorDireccion;
    socklen_t emisorTamano = sizeof(emisorDireccion);

    while (true) {
        memset(buffer, 0, sizeof(buffer)); // Limpia el buffer
        ssize_t bytesRecibidos = recvfrom(descriptorMonitor, buffer, 1024, 0, (sockaddr*)&emisorDireccion, &emisorTamano);
        if (bytesRecibidos > 0) {
            buffer[bytesRecibidos] = '\0'; // Añade el carácter de fin de cadena al buffer
            std::string received_data(buffer);

            // Separa el mensaje recibido en líneas
            std::vector<std::string> messages;
            std::istringstream stream(received_data);
            std::string token;
            while (std::getline(stream, token, '\n')) {
                if (!token.empty()) {
                    messages.push_back(token);
                }
            }

            // Imprime los mensajes recibidos
            for (const auto& msg : messages) {
                std::cout << "Mensaje recibido: " << msg << std::endl;
            }
        }
    }

    close(descriptorMonitor); // Cierra el socket (esto no se alcanzará en el código actual)
}

// Función principal del monitor de servidores
int main(int argc, char* argv[]) {
    // Verifica el número de argumentos y su formato
    if (argc < 3 || (argc - 1) % 2 != 0) {
        std::cerr << "Uso: " << argv[0] << " <num_servidores> <puerto1> ... <puertoN>\n";
        return 1;
    }

    int num_servers = std::stoi(argv[1]);
    if (num_servers <= 0 || argc != 2 + num_servers) {
        std::cerr << "Número de servidores inválido o número incorrecto de puertos.\n";
        return 1;
    }

    // Lee los puertos desde los argumentos
    std::vector<int> ports;
    for (int i = 2; i < argc; ++i) {
        ports.push_back(std::stoi(argv[i]));
    }

    // Inicializa el estado de los servidores
    server_active.resize(num_servers);
    for (int i = 0; i < num_servers; ++i) {
        server_active[i] = std::make_shared<std::atomic<bool>>(true);
    }

    // Inicia los hilos para los servidores
    std::vector<std::thread> server_threads;
    for (int i = 0; i < num_servers; ++i) {
        server_threads.emplace_back([i, &ports]() {
            start_server(i + 1, ports[i], server_active[i]);
        });
    }

    // Inicia el hilo para monitorizar los servidores
    std::thread monitor_thread(monitor_servers);

    // Inicia el hilo para recibir información de los servidores
    std::thread recibirHilo(recibirInformacionServidor);
    recibirHilo.detach(); // Detach para que siga corriendo en segundo plano

    // Espera a que los hilos de los servidores terminen
    for (auto& t : server_threads) {
        t.join();
    }
    monitor_thread.join(); // Espera a que el hilo del monitor termine

    return 0;
}
