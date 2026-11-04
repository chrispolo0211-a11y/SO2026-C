#include "worker.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <cmath>

namespace ConcurrencyLab {

    std::atomic<bool> g_ejecutando{false};

    void ejecutarCargaHilos(int num_hilos) {
        std::vector<std::thread> hilos;
        hilos.reserve(num_hilos);

        for (int i = 0; i < num_hilos; ++i) {
            hilos.emplace_back([]() {
                double resultado_acumulado = 0.0;
                
                while (g_ejecutando.load()) {
                    for (int j = 1; j < 1000; ++j) {
                        resultado_acumulado += std::sin(j) * std::cos(j);
                    }
                }
            });
        }

        for (auto& hilo : hilos) {
            if (hilo.joinable()) {
                hilo.join();
            }
        }
    }

    long long probarAccesoCompartido(int num_hilos, int iteraciones_por_hilo, bool usar_sincronizacion) {
        long long contador = 0;
        std::mutex mtx;
        std::vector<std::thread> hilos;
        hilos.reserve(num_hilos);

        for (int i = 0; i < num_hilos; ++i) {
            hilos.emplace_back([&contador, &mtx, iteraciones_por_hilo, usar_sincronizacion]() {
                for (int iter = 0; iter < iteraciones_por_hilo; ++iter) {
                    if (usar_sincronizacion) {
                        std::lock_guard<std::mutex> lock(mtx);
                        ++contador;
                    } else {
                        ++contador;
                    }
                }
            });
        }

        for (auto& hilo : hilos) {
            if (hilo.joinable()) {
                hilo.join();
            }
        }

        return contador;
    }

}
