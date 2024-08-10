#ifndef MENSAJE_H
#define MENSAJE_H

#include <string>

class Mensaje {
public:
    Mensaje(const std::string& contenido, const std::string& autor); // Constructor

    std::string obtenerContenido() const;
    std::string obtenerAutor() const;

private:
    std::string contenido;
    std::string autor;
};

#endif // MENSAJE_H
