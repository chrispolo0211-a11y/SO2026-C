#pragma once
#include <atomic>

namespace ConcurrencyLab {

    extern std::atomic<bool> g_ejecutando;

    void ejecutarCargaHilos(int num_hilos);

    long long probarAccesoCompartido(int num_hilos, int iteraciones_por_hilo, bool usar_sincronizacion);

}
