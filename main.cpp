#include <iostream>
#include "ClienteChat.h"
#include "ServidorChat.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <modo> <direccionIP> <puerto>\n";
        std::cerr << "Modos disponibles: servidor, cliente\n";
        return 1;
    }

    std::string modo = argv[1];

    if (modo == "servidor") {
        if (argc < 3) {
            std::cerr << "Uso: " << argv[0] << " servidor <puerto>\n";
            return 1;
        }
        int puerto = std::stoi(argv[2]);
        ServidorChat servidor(puerto);  // Inicializa el servidor con el puerto proporcionado
        servidor.iniciar();  // Inicia el servidor
    } else if (modo == "cliente") {
        if (argc < 4) {
            std::cerr << "Uso: " << argv[0] << " cliente <direccionIP> <puerto>\n";
            return 1;
        }
        std::string direccionIP = argv[2];
        int puerto = std::stoi(argv[3]);
        ClienteChat cliente(direccionIP, puerto);  // Inicializa el cliente con la dirección IP y puerto proporcionados
        cliente.conectarAlServidor();  // Conecta al servidor

        std::string mensaje;
        while (std::getline(std::cin, mensaje)) {
            cliente.manejarComando(mensaje);  // Envía el mensaje al servidor
        }

        cliente.desconectar();  // Desconecta del servidor
    } else {
        std::cerr << "Modo desconocido: " << modo << "\n";
        return 1;
    }

    return 0;
}
