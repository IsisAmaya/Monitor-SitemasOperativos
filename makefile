# Compilador y flags
CXX = g++
CXXFLAGS = -std=c++11 -Iinclude -Wall -Wextra

# Directorios
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build

# Archivos fuente y de cabecera (excluyendo MonitorServidores.cpp)
SRCS = $(filter-out $(SRC_DIR)/MonitorServidores.cpp, $(wildcard $(SRC_DIR)/*.cpp)) main.cpp
OBJS = $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

# Archivo ejecutable principal
TARGET = $(BUILD_DIR)/chat

# Archivo ejecutable del monitor
MONITOR_TARGET = monitor

# Puerto por defecto para el cliente (se puede sobrescribir al ejecutar make)
CLIENT_PORT = 12345

# Regla por defecto: compilar todo
all: $(TARGET)

# Regla para crear el directorio de compilación
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/$(SRC_DIR)

# Regla para compilar el ejecutable principal del chat
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Regla para compilar los archivos objeto
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Limpiar archivos compilados
clean:
	rm -rf $(BUILD_DIR) $(MONITOR_TARGET)

# Ejecutar el servidor
run-servidor: $(TARGET)
	./$(TARGET) servidor 12345

# Ejecutar el cliente, permitiendo especificar el puerto al ejecutar make
run-cliente: $(TARGET)
	./$(TARGET) cliente 127.0.0.1 $(CLIENT_PORT)

# Compilar el monitor por separado
$(MONITOR_TARGET): $(SRC_DIR)/MonitorServidores.cpp $(INCLUDE_DIR)/MonitorServidores.h
	$(CXX) $(CXXFLAGS) $(SRC_DIR)/MonitorServidores.cpp -o $(MONITOR_TARGET)

# Ejecutar el monitor
run-monitor: $(MONITOR_TARGET)
	@echo "Ejecutando el monitor..."
	@read -p "Ingrese la cantidad de servidores: " NUM_SERVERS; \
	read -p "Ingrese los puertos (separados por espacio): " PORTS; \
	./$(MONITOR_TARGET) $$NUM_SERVERS $$PORTS


# Declarar reglas como phony
.PHONY: all clean run-servidor run-cliente monitor run-monitor
