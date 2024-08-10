#include "ClienteChat.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>

// Constructor que inicializa la dirección IP y el puerto del servidor
ClienteChat::ClienteChat(const std::string& direccionIP, int puerto)
    : direccionIP(direccionIP), puerto(puerto), descriptorCliente(-1), conectado(false) {}

// Método para conectar al servidor
void ClienteChat::conectarAlServidor() {
    // Crear el socket del cliente
    descriptorCliente = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptorCliente == -1) {
        std::cerr << "Error al crear el socket del cliente.\n";
        return;
    }

    sockaddr_in direccionServidor;
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(puerto);
    inet_pton(AF_INET, direccionIP.c_str(), &direccionServidor.sin_addr);

    // Conectar al servidor
    if (connect(descriptorCliente, (sockaddr*)&direccionServidor, sizeof(direccionServidor)) == -1) {
        std::cerr << "Error al conectar al servidor.\n";
        return;
    }

    conectado = true;

    // Iniciar un hilo para recibir mensajes del servidor
    std::thread hiloRecibir(&ClienteChat::recibirMensajes, this);
    hiloRecibir.detach();
}

// Método para manejar los comandos del usuario y enviarlos al servidor
void ClienteChat::manejarComando(const std::string& comando) {
    if (conectado) {
        send(descriptorCliente, comando.c_str(), comando.size(), 0);
    }
}

// Método para desconectar del servidor
void ClienteChat::desconectar() {
    if (conectado) {
        close(descriptorCliente);
        conectado = false;
    }
}

// Método para recibir mensajes del servidor
void ClienteChat::recibirMensajes() {
    char buffer[1024];
    while (conectado) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRecibidos = recv(descriptorCliente, buffer, 1024, 0);
        if (bytesRecibidos <= 0) {
            std::cerr << "Desconectado del servidor.\n";
            desconectar();
            break;
        }
        std::cout << std::string(buffer, bytesRecibidos) << std::endl;
    }
}
