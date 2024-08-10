#ifndef SERVIDORCHAT_H
#define SERVIDORCHAT_H

#include "Usuario.h"
#include <string>
#include <vector>
#include <mutex>

class ServidorChat {
public:
    ServidorChat(int puerto);
    void iniciar();

private:
    void manejarCliente(int descriptorCliente);
    void enviarMensajeATodos(const std::string& mensaje, int descriptorRemitente);
    void enviarListaUsuarios(int descriptorCliente);
    void enviarDetallesConexion(int descriptorCliente);

    int puerto;  // Puerto en el que escucha el servidor
    int descriptorServidor;  // Descriptor del socket del servidor
    std::vector<Usuario> usuarios;  // Lista de usuarios conectados
    std::mutex mutexUsuarios;  // Mutex para proteger el acceso a la lista de usuarios
};

#endif // SERVIDORCHAT_H
