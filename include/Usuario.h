#ifndef USUARIO_H
#define USUARIO_H

#include <string>

class Usuario {
public:
    Usuario(const std::string& nombreUsuario, int descriptorSocket);
    std::string obtenerNombreUsuario() const;
    int obtenerDescriptorSocket() const;

private:
    std::string nombreUsuario;  // Nombre del usuario
    int descriptorSocket;      // Descriptor del socket del usuario
};

#endif // USUARIO_H
