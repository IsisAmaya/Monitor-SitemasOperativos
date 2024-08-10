#include "Usuario.h"

// Constructor que inicializa el nombre del usuario y el descriptor del socket
Usuario::Usuario(const std::string& nombreUsuario, int descriptorSocket)
    : nombreUsuario(nombreUsuario), descriptorSocket(descriptorSocket) {}

// Método para obtener el nombre del usuario
std::string Usuario::obtenerNombreUsuario() const {
    return nombreUsuario;
}

// Método para obtener el descriptor del socket del usuario
int Usuario::obtenerDescriptorSocket() const {
    return descriptorSocket;
}
