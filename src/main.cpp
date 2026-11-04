#include "memory.hpp"
#include "ui.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>

namespace {
    bool leerEnteroPositivo(const char* texto, int& resultado) {
        char* final = nullptr;
        long valor = std::strtol(texto, &final, 10);
        if (final == texto || *final != '\0' || valor <= 0 ||
            valor > static_cast<long>(std::numeric_limits<int>::max())) {
            return false;
        }
        resultado = static_cast<int>(valor);
        return true;
    }

    bool leerTamanoPositivo(const char* texto, size_t& resultado) {
        char* final = nullptr;
        unsigned long long valor = std::strtoull(texto, &final, 10);
        if (final == texto || *final != '\0' || valor == 0 ||
            valor > static_cast<unsigned long long>(
                         std::numeric_limits<size_t>::max())) {
            return false;
        }
        resultado = static_cast<size_t>(valor);
        return true;
    }

    bool cargarConfiguracion(int argc, char** argv,
                             ConcurrencyLab::ConfiguracionPrueba& configuracion) {
        if (argc == 1) {
            return true;
        }
        if (argc != 6) {
            std::cerr << "Uso: " << argv[0]
                      << " <memoria_mb> <cpu_s> <limite_memoria_mb>"
                      << " <duracion_s> <intervalo_ms>\n";
            return false;
        }
        return leerTamanoPositivo(argv[1], configuracion.memoria_mb) &&
               leerEnteroPositivo(argv[2], configuracion.limite_cpu_segundos) &&
               leerTamanoPositivo(argv[3], configuracion.limite_memoria_mb) &&
               leerEnteroPositivo(argv[4], configuracion.duracion_segundos) &&
               leerEnteroPositivo(argv[5], configuracion.intervalo_milisegundos);
    }
}

int main(int argc, char** argv) {
    ConcurrencyLab::ConfiguracionPrueba configuracion{150, 15, 300, 3, 500};
    if (argc == 1) {
        return ConcurrencyLab::ejecutarInterfaz(configuracion);
    }
    if (!cargarConfiguracion(argc, argv, configuracion)) {
        return 2;
    }

    std::cout << "========================================\n"
              << "   MONITOR DE MEMORIA DEL PROCESO\n"
              << "========================================\n\n";
    ConcurrencyLab::mostrarConfiguracion(configuracion);

    auto limites = ConcurrencyLab::aplicarLimitesRecursos(
        configuracion.limite_cpu_segundos, configuracion.limite_memoria_mb);
    if (!limites.exito_cpu || !limites.exito_memoria) {
        std::cerr << "No se pudieron aplicar los limites: "
                  << limites.mensaje_error << '\n';
        return 1;
    }

    std::cout << "Metricas iniciales:\n";
    ConcurrencyLab::mostrarMetricas(ConcurrencyLab::obtenerMetricasProceso());

    std::cout << "\nReservando memoria con mmap...\n";
    auto reserva = ConcurrencyLab::iniciarPresionMemoria(
        configuracion.memoria_mb);
    if (!reserva.exito) {
        std::cerr << "No se pudo reservar memoria: "
                  << reserva.mensaje_error << '\n';
        return 1;
    }

    std::cout << "Reserva exitosa: " << reserva.bytes_reservados
              << " bytes\n\nMonitoreo en tiempo real:\n";
    const auto intervalo =
        std::chrono::milliseconds(configuracion.intervalo_milisegundos);
    const auto fin = std::chrono::steady_clock::now() +
                     std::chrono::seconds(configuracion.duracion_segundos);
    while (std::chrono::steady_clock::now() < fin) {
        ConcurrencyLab::mostrarMetricas(
            ConcurrencyLab::obtenerMetricasProceso());
        std::this_thread::sleep_for(intervalo);
    }

    ConcurrencyLab::liberarPresionMemoria();
    std::cout << "\nMemoria liberada. Metricas finales:\n";
    ConcurrencyLab::mostrarMetricas(ConcurrencyLab::obtenerMetricasProceso());
    return 0;
}
