#include "memory.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <cstring>
#include <limits>
#include <mutex>
#include <unistd.h>

namespace ConcurrencyLab {

    namespace {
        void* g_bloque_memoria = nullptr;
        size_t g_tamano_reservado = 0;
        std::mutex g_mutex_memoria;
        std::mutex g_mutex_limites;

        void liberarPresionMemoriaSinBloqueo() {
            if (g_bloque_memoria != nullptr) {
                munmap(g_bloque_memoria, g_tamano_reservado);
                g_bloque_memoria = nullptr;
                g_tamano_reservado = 0;
            }
        }

        void agregarError(std::string& destino, const char* operacion) {
            if (!destino.empty()) {
                destino += "; ";
            }
            destino += operacion;
            destino += ": ";
            destino += std::strerror(errno);
        }
    }

    SystemMetrics obtenerMetricasProceso() {
        SystemMetrics metricas{0, 0, 0, 0};
        std::ifstream archivo("/proc/self/status");
        std::string linea;
        while (std::getline(archivo, linea)) {
            std::istringstream entrada(linea);
            std::string clave;
            unsigned long long valor = 0;
            std::string unidad;
            if (!(entrada >> clave >> valor >> unidad)) {
                continue;
            }
            if (clave == "VmSize:") {
                metricas.virtual_memory_kb = valor;
            } else if (clave == "VmRSS:") {
                metricas.resident_memory_kb = valor;
            } else if (clave == "VmSwap:") {
                metricas.swap_kb = valor;
            }
        }

        struct rusage uso;
        if (getrusage(RUSAGE_SELF, &uso) == 0) {
            metricas.page_faults =
                static_cast<unsigned long long>(uso.ru_minflt) +
                static_cast<unsigned long long>(uso.ru_majflt);
        }

        return metricas;
    }

    ResultadoMemoria iniciarPresionMemoria(size_t megabytes_a_reservar) {
        std::lock_guard<std::mutex> bloqueo(g_mutex_memoria);
        liberarPresionMemoriaSinBloqueo();

        ResultadoMemoria resultado{false, 0, ""};
        constexpr size_t bytes_por_megabyte = 1024 * 1024;
        if (megabytes_a_reservar == 0 ||
            megabytes_a_reservar > std::numeric_limits<size_t>::max() /
                                       bytes_por_megabyte) {
            resultado.mensaje_error = "Tamaño de reserva inválido";
            return resultado;
        }

        const size_t tamano = megabytes_a_reservar * bytes_por_megabyte;
        const long tamano_pagina = sysconf(_SC_PAGESIZE);
        if (tamano_pagina <= 0) {
            resultado.mensaje_error = "sysconf(_SC_PAGESIZE): tamaño inválido";
            return resultado;
        }

        void* bloque = mmap(nullptr, tamano, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (bloque == MAP_FAILED) {
            resultado.mensaje_error = "mmap: ";
            resultado.mensaje_error += std::strerror(errno);
            return resultado;
        }

        g_bloque_memoria = bloque;
        g_tamano_reservado = tamano;
        char* ptr = static_cast<char*>(g_bloque_memoria);
        const size_t paso = static_cast<size_t>(tamano_pagina);
        for (size_t i = 0; i < g_tamano_reservado;) {
            ptr[i] = 1;
            if (g_tamano_reservado - i <= paso) {
                break;
            }
            i += paso;
        }
        resultado.exito = true;
        resultado.bytes_reservados = tamano;
        return resultado;
    }

    void liberarPresionMemoria() {
        std::lock_guard<std::mutex> bloqueo(g_mutex_memoria);
        liberarPresionMemoriaSinBloqueo();
    }
    
    ResultadoLimite aplicarLimitesRecursos(int limite_cpu_segundos, size_t limite_memoria_mb) {
        std::lock_guard<std::mutex> bloqueo(g_mutex_limites);
        ResultadoLimite resultado{true, true, ""};

        if (limite_cpu_segundos > 0) {
            struct rlimit limit_cpu{};
            if (static_cast<rlim_t>(limite_cpu_segundos) >
                std::numeric_limits<rlim_t>::max() - 5) {
                resultado.exito_cpu = false;
                resultado.mensaje_error = "RLIMIT_CPU: tamaño fuera de rango";
                return resultado;
            }
            limit_cpu.rlim_cur = static_cast<rlim_t>(limite_cpu_segundos);
            limit_cpu.rlim_max = static_cast<rlim_t>(limite_cpu_segundos) + 5;
            if (setrlimit(RLIMIT_CPU, &limit_cpu) != 0) {
                resultado.exito_cpu = false;
                agregarError(resultado.mensaje_error, "RLIMIT_CPU");
            }
        }

        if (limite_memoria_mb > 0) {
            constexpr size_t bytes_por_megabyte = 1024 * 1024;
            if (limite_memoria_mb > std::numeric_limits<rlim_t>::max() /
                                       bytes_por_megabyte) {
                resultado.exito_memoria = false;
                resultado.mensaje_error +=
                    (resultado.mensaje_error.empty() ? "" : "; ");
                resultado.mensaje_error += "RLIMIT_AS: tamaño fuera de rango";
                return resultado;
            }

            struct rlimit limit_mem{};
            rlim_t bytes = static_cast<rlim_t>(limite_memoria_mb) * 1024 * 1024;
            limit_mem.rlim_cur = bytes;
            limit_mem.rlim_max = bytes;
            if (setrlimit(RLIMIT_AS, &limit_mem) != 0) {
                resultado.exito_memoria = false;
                agregarError(resultado.mensaje_error, "RLIMIT_AS");
            }
        }

        return resultado;
    }
}