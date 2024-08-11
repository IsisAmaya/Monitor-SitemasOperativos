#ifndef SERVIDORCHAT_H
#define SERVIDORCHAT_H

#include "Usuario.h"
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

class ServidorChat {
public:
    ServidorChat(int puerto);
    void iniciar();

private:
    void manejarCliente(int descriptorCliente);
    void enviarMensajeATodos(const std::string& mensaje, int descriptorRemitente);
    void enviarListaUsuarios(int descriptorCliente);
    void enviarDetallesConexion(int descriptorCliente);
    void enviarInformacionMonitor();

    // Nuevas variables para métricas
    void actualizarEstadisticas(const std::chrono::steady_clock::time_point& tiempoMensaje);
    void calcularYEnviarEstadisticas();

    int puerto;  // Puerto en el que escucha el servidor
    int descriptorServidor;  // Descriptor del socket del servidor
    std::vector<Usuario> usuarios;  // Lista de usuarios conectados
    std::mutex mutexUsuarios;  // Mutex para proteger el acceso a la lista de usuarios

    // Variables para calcular métricas
    int totalMensajes;  // Número total de mensajes recibidos
    std::chrono::steady_clock::time_point tiempoUltimoMensaje;  // Tiempo del último mensaje recibido
    std::chrono::duration<double> tiempoTotal;  // Tiempo total acumulado
    int totalUsuarios;  // Número total de usuarios conectados
    double promedioMensajes;  // Promedio de mensajes por segundo
    double tasaUso;  // Tasa de uso (mensajes por usuario)
};

#endif // SERVIDORCHAT_H
