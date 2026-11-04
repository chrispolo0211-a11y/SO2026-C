#pragma once
#include <cstddef>
#include <string>

namespace ConcurrencyLab {

    struct SystemMetrics {
        unsigned long long virtual_memory_kb;  // VmSize
        unsigned long long resident_memory_kb; // VmRSS
        unsigned long long swap_kb;             // VmSwap
        unsigned long long page_faults;         // Major + Minor faults
    };
    struct ResultadoLimite {
        bool exito_cpu;
        bool exito_memoria;
        std::string mensaje_error;
    };
    struct ResultadoMemoria {
        bool exito;
        size_t bytes_reservados;
        std::string mensaje_error;
    };

    // Parsea /proc/self/status y extrae el estado actual del proceso
    SystemMetrics obtenerMetricasProceso();

    // Asigna y toca páginas de memoria para generar presión residente.
    // Una nueva reserva reemplaza de forma segura la reserva anterior.
    ResultadoMemoria iniciarPresionMemoria(size_t megabytes_a_reservar);
    void liberarPresionMemoria();
    ResultadoLimite aplicarLimitesRecursos(int limite_cpu_segundos, size_t limite_memoria_mb);
}
