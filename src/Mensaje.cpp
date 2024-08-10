#include "Mensaje.h"

Mensaje::Mensaje(const std::string& contenido, const std::string& autor)
    : contenido(contenido), autor(autor) {}

std::string Mensaje::obtenerContenido() const {
    return contenido;
}

std::string Mensaje::obtenerAutor() const {
    return autor;
}
