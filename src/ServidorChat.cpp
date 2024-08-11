#include "ServidorChat.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>

// Constructor que inicializa el puerto del servidor
ServidorChat::ServidorChat(int puerto)
    : puerto(puerto), descriptorServidor(-1) {}

// Método para iniciar el servidor
void ServidorChat::iniciar() {
    // Crear el socket del servidor
    descriptorServidor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptorServidor == -1) {
        std::cerr << "Error al crear el socket del servidor.\n";
        return;
    }
// Configurar SO_REUSEADDR y SO_REUSEPORT
int opt = 1;
if (setsockopt(descriptorServidor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
    std::cerr << "Error al configurar el socket con SO_REUSEADDR | SO_REUSEPORT.\n";
    close(descriptorServidor);
    return;
}

    sockaddr_in direccionServidor;
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(puerto);
    direccionServidor.sin_addr.s_addr = INADDR_ANY;

    // Asociar el socket a la dirección y puerto
    if (bind(descriptorServidor, (sockaddr*)&direccionServidor, sizeof(direccionServidor)) == -1) {
        std::cerr << "Error al hacer bind del socket del servidor.\n";
        return;
    }

    // Poner el servidor en modo escucha
    if (listen(descriptorServidor, 10) == -1) {
        std::cerr << "Error al poner el servidor en modo escucha.\n";
        return;
    }

    std::cout << "Servidor iniciado en el puerto " << puerto << ". Esperando conexiones...\n";

    std::thread([this]() {
        while (true) {
            enviarInformacionMonitor();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }).detach();    

    // Aceptar conexiones entrantes
    while (true) {
        sockaddr_in direccionCliente;
        socklen_t tamanoDireccionCliente = sizeof(direccionCliente);
        int descriptorCliente = accept(descriptorServidor, (sockaddr*)&direccionCliente, &tamanoDireccionCliente);

        if (descriptorCliente == -1) {
            std::cerr << "Error al aceptar la conexión de un cliente.\n";
            continue;
        }

        // Crear un hilo para manejar el cliente
        std::thread hiloCliente(&ServidorChat::manejarCliente, this, descriptorCliente);
        hiloCliente.detach();
    }
}

// Manejar la comunicación con un cliente
void ServidorChat::manejarCliente(int descriptorCliente) {
    char buffer[1024];
    std::string nombreUsuario;

    // Solicitar el nombre del usuario
    send(descriptorCliente, "Ingrese su nombre: ", 20, 0);
    ssize_t bytesRecibidos = recv(descriptorCliente, buffer, 1024, 0);
    if (bytesRecibidos <= 0) {
        close(descriptorCliente);
        return;
    }

    nombreUsuario = std::string(buffer, bytesRecibidos);
    nombreUsuario.erase(nombreUsuario.find_last_not_of(" \n\r\t") + 1); // Eliminar espacios en blanco

    {
        std::lock_guard<std::mutex> lock(mutexUsuarios);
        usuarios.emplace_back(nombreUsuario, descriptorCliente);
    }

    // Notificar a todos los usuarios que un nuevo usuario se ha conectado
    std::string mensajeBienvenida = nombreUsuario + " se ha conectado al chat.\n";
    enviarMensajeATodos(mensajeBienvenida, descriptorCliente);

    // Manejar los mensajes del cliente
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        bytesRecibidos = recv(descriptorCliente, buffer, 1024, 0);

        if (bytesRecibidos <= 0) {
            // El cliente se ha desconectado
            {
                std::lock_guard<std::mutex> lock(mutexUsuarios);
                for (auto it = usuarios.begin(); it != usuarios.end(); ++it) {
                    if (it->obtenerDescriptorSocket() == descriptorCliente) {
                        std::string mensajeDespedida = it->obtenerNombreUsuario() + " se ha desconectado del chat.\n";
                        enviarMensajeATodos(mensajeDespedida, descriptorCliente);
                        usuarios.erase(it);
                        break;
                    }
                }
            }
            close(descriptorCliente);
            break;
        }

        std::string mensaje = std::string(buffer, bytesRecibidos);

        // Procesar comandos del protocolo
        if (mensaje.substr(0, 9) == "@usuarios") {
            enviarListaUsuarios(descriptorCliente);
        } else if (mensaje.substr(0, 9) == "@conexion") {
            enviarDetallesConexion(descriptorCliente);
        } else if (mensaje.substr(0, 6) == "@salir") {
            close(descriptorCliente);
            break;
        } else if (mensaje.substr(0, 2) == "@h") {
            std::string ayuda = "Comandos disponibles:\n"
                                "@usuarios - Lista de usuarios conectados\n"
                                "@conexion - Muestra la conexión y el número de usuarios\n"
                                "@salir - Desconectar del chat\n";
            send(descriptorCliente, ayuda.c_str(), ayuda.size(), 0);
        } else {
            // Enviar el mensaje a todos los usuarios
            mensaje = nombreUsuario + ": " + mensaje;
            enviarMensajeATodos(mensaje, descriptorCliente);
        }
    }
}

// Enviar un mensaje a todos los usuarios conectados, excepto al remitente
void ServidorChat::enviarMensajeATodos(const std::string& mensaje, int descriptorRemitente) {
    std::lock_guard<std::mutex> lock(mutexUsuarios);
    for (const auto& usuario : usuarios) {
        if (usuario.obtenerDescriptorSocket() != descriptorRemitente) {
            send(usuario.obtenerDescriptorSocket(), mensaje.c_str(), mensaje.size(), 0);
        }
    }
}

// Enviar la lista de usuarios conectados al cliente especificado
void ServidorChat::enviarListaUsuarios(int descriptorCliente) {
    std::lock_guard<std::mutex> lock(mutexUsuarios);
    std::string listaUsuarios = "Usuarios conectados:\n";
    for (const auto& usuario : usuarios) {
        listaUsuarios += usuario.obtenerNombreUsuario() + "\n";
    }
    send(descriptorCliente, listaUsuarios.c_str(), listaUsuarios.size(), 0);
}

// Enviar los detalles de la conexión y el número de usuarios conectados
void ServidorChat::enviarDetallesConexion(int descriptorCliente) {
    std::lock_guard<std::mutex> lock(mutexUsuarios);
    std::string detalles = "Número de usuarios conectados: " + std::to_string(usuarios.size()) + "\n";
    send(descriptorCliente, detalles.c_str(), detalles.size(), 0);
}


void ServidorChat::enviarInformacionMonitor() {
    int Socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (Socket == -1) {
        std::cerr << "Error al crear el socket UDP.\n";
        return;
    }

    sockaddr_in direccionMonitor;
    direccionMonitor.sin_family = AF_INET;
    direccionMonitor.sin_port = htons(55555); // Puerto para el monitor
    direccionMonitor.sin_addr.s_addr = inet_addr("127.0.0.1"); // Dirección IP del monitor (localhost)

    //Obtener numero de usuarios
    std::lock_guard<std::mutex> lock(mutexUsuarios);
    int numeroUsuarios = usuarios.size();
    std::string mensaje = "Usuarios conectados: " + std::to_string(numeroUsuarios);

    std::string test = "esto es una prueba";

    std::vector<std::string> messages = {mensaje, test};

    // Concatenar las cadenas en un solo buffer con delimitadores
    std::string mensajesConcatenados;
    std::string delimiter = "\n"; // Usamos '\n' como delimitador
    for (const auto& msg : messages) {
        mensajesConcatenados += msg + delimiter;
    }

    // Convertir el string concatenado a bytes
    const char* data = mensajesConcatenados.c_str();
    size_t data_length = mensajesConcatenados.size();


    sendto(Socket, data, data_length, 0, (struct sockaddr*)&direccionMonitor, sizeof(direccionMonitor));
    close(Socket);
}
