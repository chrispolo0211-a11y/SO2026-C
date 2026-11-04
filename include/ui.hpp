#pragma once

#include "memory.hpp"
#include <cstddef>

namespace ConcurrencyLab {

    struct ConfiguracionPrueba {
        size_t memoria_mb;
        int limite_cpu_segundos;
        size_t limite_memoria_mb;
        int duracion_segundos;
        int intervalo_milisegundos;
    };

    void mostrarConfiguracion(const ConfiguracionPrueba& configuracion);
    void mostrarMetricas(const SystemMetrics& metricas);
    int ejecutarInterfaz(ConfiguracionPrueba configuracion);
}
