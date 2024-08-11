#include "ServidorChat.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>

// Constructor de la clase ServidorChat
ServidorChat::ServidorChat(int puerto)
    : puerto(puerto), descriptorServidor(-1), totalMensajes(0), 
      tiempoUltimoMensaje(std::chrono::steady_clock::now()), 
      tiempoTotal(std::chrono::duration<double>::zero()), 
      totalUsuarios(0), promedioMensajes(0.0), tasaUso(0.0) {}

// Método para iniciar el servidor
void ServidorChat::iniciar() {
    // Crea un socket para el servidor
    descriptorServidor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptorServidor == -1) {
        std::cerr << "Error al crear el socket del servidor.\n";
        return;
    }

    // Configura el socket para reutilizar la dirección y el puerto
    int opt = 1;
    if (setsockopt(descriptorServidor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        std::cerr << "Error al configurar el socket con SO_REUSEADDR | SO_REUSEPORT.\n";
        close(descriptorServidor);
        return;
    }

    // Configura la dirección del servidor
    sockaddr_in direccionServidor;
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(puerto);
    direccionServidor.sin_addr.s_addr = INADDR_ANY;

    // Asocia el socket con la dirección y el puerto
    if (bind(descriptorServidor, (sockaddr*)&direccionServidor, sizeof(direccionServidor)) == -1) {
        std::cerr << "Error al hacer bind del socket del servidor.\n";
        return;
    }

    // Pone el servidor en modo escucha
    if (listen(descriptorServidor, 10) == -1) {
        std::cerr << "Error al poner el servidor en modo escucha.\n";
        return;
    }

    std::cout << "Servidor iniciado en el puerto " << puerto << ". Esperando conexiones...\n";

    // Crea un hilo para calcular y enviar estadísticas
    std::thread([this]() {
        while (true) {
            calcularYEnviarEstadisticas();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }).detach();    

    // Acepta conexiones de clientes en un bucle infinito
    while (true) {
        sockaddr_in direccionCliente;
        socklen_t tamanoDireccionCliente = sizeof(direccionCliente);
        int descriptorCliente = accept(descriptorServidor, (sockaddr*)&direccionCliente, &tamanoDireccionCliente);

        if (descriptorCliente == -1) {
            std::cerr << "Error al aceptar la conexión de un cliente.\n";
            continue;
        }

        // Crea un hilo para manejar la conexión del cliente
        std::thread hiloCliente(&ServidorChat::manejarCliente, this, descriptorCliente);
        hiloCliente.detach();
    }
}

// Maneja la conexión con un cliente específico
void ServidorChat::manejarCliente(int descriptorCliente) {
    char buffer[1024];
    std::string nombreUsuario;

    // Pide al cliente que ingrese su nombre
    send(descriptorCliente, "Ingrese su nombre: ", 20, 0);
    ssize_t bytesRecibidos = recv(descriptorCliente, buffer, 1024, 0);
    if (bytesRecibidos <= 0) {
        close(descriptorCliente);
        return;
    }

    // Limpia el nombre del usuario y lo almacena
    nombreUsuario = std::string(buffer, bytesRecibidos);
    nombreUsuario.erase(nombreUsuario.find_last_not_of(" \n\r\t") + 1);

    {
        std::lock_guard<std::mutex> lock(mutexUsuarios);
        usuarios.emplace_back(nombreUsuario, descriptorCliente);
        totalUsuarios = usuarios.size();
    }

    // Envía un mensaje de bienvenida a todos los usuarios
    std::string mensajeBienvenida = nombreUsuario + " se ha conectado al chat.\n";
    enviarMensajeATodos(mensajeBienvenida, descriptorCliente);

    // Maneja los mensajes del cliente en un bucle
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        bytesRecibidos = recv(descriptorCliente, buffer, 1024, 0);

        if (bytesRecibidos <= 0) {
            {
                std::lock_guard<std::mutex> lock(mutexUsuarios);
                for (auto it = usuarios.begin(); it != usuarios.end(); ++it) {
                    if (it->obtenerDescriptorSocket() == descriptorCliente) {
                        std::string mensajeDespedida = it->obtenerNombreUsuario() + " se ha desconectado del chat.\n";
                        enviarMensajeATodos(mensajeDespedida, descriptorCliente);
                        usuarios.erase(it);
                        totalUsuarios = usuarios.size();
                        break;
                    }
                }
            }
            close(descriptorCliente);
            break;
        }

        std::string mensaje = std::string(buffer, bytesRecibidos);
        actualizarEstadisticas(std::chrono::steady_clock::now());

        // Maneja comandos específicos del chat
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
            mensaje = nombreUsuario + ": " + mensaje;
            enviarMensajeATodos(mensaje, descriptorCliente);
        }
    }
}

// Envía un mensaje a todos los usuarios conectados, excepto al remitente
void ServidorChat::enviarMensajeATodos(const std::string& mensaje, int descriptorRemitente) {
    std::lock_guard<std::mutex> lock(mutexUsuarios);
    for (const auto& usuario : usuarios) {
        if (usuario.obtenerDescriptorSocket() != descriptorRemitente) {
            send(usuario.obtenerDescriptorSocket(), mensaje.c_str(), mensaje.size(), 0);
        }
    }
}

// Envía la lista de usuarios conectados al cliente especificado
void ServidorChat::enviarListaUsuarios(int descriptorCliente) {
    std::lock_guard<std::mutex> lock(mutexUsuarios);
    std::string listaUsuarios = "Usuarios conectados:\n";
    for (const auto& usuario : usuarios) {
        listaUsuarios += usuario.obtenerNombreUsuario() + "\n";
    }
    send(descriptorCliente, listaUsuarios.c_str(), listaUsuarios.size(), 0);
}

// Envía detalles de la conexión y el número de usuarios conectados al cliente especificado
void ServidorChat::enviarDetallesConexion(int descriptorCliente) {
    std::lock_guard<std::mutex> lock(mutexUsuarios);
    std::string detalles = "Número de usuarios conectados: " + std::to_string(usuarios.size()) + "\n";
    send(descriptorCliente, detalles.c_str(), detalles.size(), 0);
}

// Envía información al monitor a través de un socket UDP
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

    std::lock_guard<std::mutex> lock(mutexUsuarios);
    std::string mensaje = "Usuarios conectados: " + std::to_string(totalUsuarios) + "\n";
    mensaje += "Promedio de mensajes por segundo: " + std::to_string(promedioMensajes) + "\n";
    mensaje += "Tasa de uso (mensajes por usuario): " + std::to_string(tasaUso) + "\n";
    mensaje += "Tiempo entre mensajes (segundos): " + std::to_string(tiempoTotal.count() / totalMensajes) + "\n";

    const char* data = mensaje.c_str();
    size_t data_length = mensaje.size();

    sendto(Socket, data, data_length, 0, (struct sockaddr*)&direccionMonitor, sizeof(direccionMonitor));
    close(Socket);
}

// Actualiza las estadísticas del servidor basándose en el tiempo del último mensaje
void ServidorChat::actualizarEstadisticas(const std::chrono::steady_clock::time_point& tiempoMensaje) {
    totalMensajes++;
    if (totalMensajes > 1) {
        auto duracionEntreMensajes = std::chrono::duration_cast<std::chrono::duration<double>>(tiempoMensaje - tiempoUltimoMensaje);
        tiempoTotal += duracionEntreMensajes;
        promedioMensajes = totalMensajes / tiempoTotal.count();
        tasaUso = static_cast<double>(totalMensajes) / totalUsuarios;
        tiempoUltimoMensaje = tiempoMensaje;
    } else {
        tiempoUltimoMensaje = tiempoMensaje;
    }
}

// Llama al método para enviar información al monitor
void ServidorChat::calcularYEnviarEstadisticas() {
    enviarInformacionMonitor();
}
