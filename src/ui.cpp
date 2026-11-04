#include "ui.hpp"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <ncurses.h>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#include <thread>

namespace ConcurrencyLab {

    void mostrarConfiguracion(const ConfiguracionPrueba& configuracion) {
        std::cout << "Configuracion de la prueba:\n"
                  << "  Memoria a reservar: " << configuracion.memoria_mb << " MB\n"
                  << "  Limite de CPU: " << configuracion.limite_cpu_segundos << " s\n"
                  << "  Limite de memoria virtual: "
                  << configuracion.limite_memoria_mb << " MB\n"
                  << "  Duracion: " << configuracion.duracion_segundos << " s\n"
                  << "  Intervalo de monitoreo: "
                  << configuracion.intervalo_milisegundos << " ms\n\n";
    }

    void mostrarMetricas(const SystemMetrics& metricas) {
        std::cout << "  VmSize: " << metricas.virtual_memory_kb << " KB"
                  << " | VmRSS: " << metricas.resident_memory_kb << " KB"
                  << " | VmSwap: " << metricas.swap_kb << " KB"
                  << " | Fallos de pagina: " << metricas.page_faults << '\n';
    }

    int ejecutarInterfaz(ConfiguracionPrueba configuracion) {
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);
        start_color();
        init_pair(1, COLOR_BLACK, COLOR_CYAN);
        init_pair(2, COLOR_CYAN, COLOR_BLACK);
        init_pair(3, COLOR_BLACK, COLOR_GREEN);
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(5, COLOR_RED, COLOR_BLACK);
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
        timeout(200);

        const char* nombres[] = {
            "Memoria mmap (MB)", "Limite CPU (s)", "Limite memoria (MB)",
            "Duracion (s)", "Intervalo (ms)"
        };
        int campo = 0;
        bool ejecutando = false;
        bool salir = false;
        bool error = false;
        std::string mensaje;
        SystemMetrics metricas{0, 0, 0, 0};
        pid_t proceso_prueba = -1;
        int descriptor_metricas = -1;

        struct MensajeMonitor {
            SystemMetrics metricas;
            int estado;
            char mensaje[256];
        };

        auto cerrarMonitor = [&]() {
            if (descriptor_metricas >= 0) {
                close(descriptor_metricas);
                descriptor_metricas = -1;
            }
            if (proceso_prueba > 0) {
                waitpid(proceso_prueba, nullptr, 0);
                proceso_prueba = -1;
            }
            ejecutando = false;
        };

        auto editarCampo = [&]() {
            char entrada[32] = {};
            timeout(-1);
            curs_set(1);
            mvprintw(LINES - 3, 3, "Nuevo valor para %s: ", nombres[campo]);
            clrtoeol();
            refresh();
            int longitud = 0;
            while (longitud < static_cast<int>(sizeof(entrada) - 1)) {
                int tecla_entrada = getch();
                if (tecla_entrada == '\n' || tecla_entrada == KEY_ENTER) {
                    break;
                }
                if ((tecla_entrada == KEY_BACKSPACE ||
                     tecla_entrada == 127 || tecla_entrada == 8) &&
                    longitud > 0) {
                    --longitud;
                    entrada[longitud] = '\0';
                    addch('\b');
                    addch(' ');
                    addch('\b');
                    refresh();
                    continue;
                }
                if (tecla_entrada >= '0' && tecla_entrada <= '9') {
                    entrada[longitud++] = static_cast<char>(tecla_entrada);
                    entrada[longitud] = '\0';
                    addch(tecla_entrada);
                    refresh();
                }
            }
            curs_set(0);
            timeout(200);
            if (longitud <= 0) {
                mensaje = "No se ingreso ningun valor";
                error = true;
                return;
            }

            char* final = nullptr;
            unsigned long long valor = std::strtoull(entrada, &final, 10);
            if (*final != '\0' || valor == 0 ||
                valor > static_cast<unsigned long long>(
                             std::numeric_limits<size_t>::max())) {
                mensaje = "Valor invalido; debe ser un entero positivo";
                error = true;
                return;
            }

            if (campo == 0) {
                configuracion.memoria_mb = static_cast<size_t>(valor);
            } else if (campo == 1) {
                if (valor > static_cast<unsigned long long>(
                                std::numeric_limits<int>::max())) {
                    mensaje = "El limite de CPU es demasiado grande";
                    error = true;
                    return;
                }
                configuracion.limite_cpu_segundos = static_cast<int>(valor);
            } else if (campo == 2) {
                configuracion.limite_memoria_mb = static_cast<size_t>(valor);
            } else if (campo == 3) {
                if (valor > static_cast<unsigned long long>(
                                std::numeric_limits<int>::max())) {
                    mensaje = "La duracion es demasiado grande";
                    error = true;
                    return;
                }
                configuracion.duracion_segundos = static_cast<int>(valor);
            } else {
                if (valor > static_cast<unsigned long long>(
                                std::numeric_limits<int>::max())) {
                    mensaje = "El intervalo es demasiado grande";
                    error = true;
                    return;
                }
                configuracion.intervalo_milisegundos = static_cast<int>(valor);
            }
            error = false;
            mensaje = "Valor actualizado";
        };

        while (!salir) {
            erase();
            attron(COLOR_PAIR(1) | A_BOLD);
            mvhline(0, 0, ' ', COLS);
            mvprintw(1, 2, " LABORATORIO DE CONCURRENCIA - MONITOR DE MEMORIA ");
            attroff(COLOR_PAIR(1) | A_BOLD);
            attron(COLOR_PAIR(2));
            mvhline(2, 0, ACS_HLINE, COLS);
            attroff(COLOR_PAIR(2));

            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(4, 3, "CONFIGURACION");
            attroff(COLOR_PAIR(2) | A_BOLD);
            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(4, COLS / 2 + 2, "TELEMETRIA EN VIVO");
            attroff(COLOR_PAIR(6) | A_BOLD);

            long long valores[] = {
                static_cast<long long>(configuracion.memoria_mb),
                configuracion.limite_cpu_segundos,
                static_cast<long long>(configuracion.limite_memoria_mb),
                configuracion.duracion_segundos,
                configuracion.intervalo_milisegundos
            };
            for (int i = 0; i < 5; ++i) {
                attron(COLOR_PAIR(i == campo && !ejecutando ? 3 : 2));
                if (!ejecutando && i == campo) attron(A_BOLD);
                mvprintw(6 + i * 2, 4, "%-24s %lld", nombres[i], valores[i]);
                if (!ejecutando && i == campo) attroff(A_BOLD);
                attroff(COLOR_PAIR(i == campo && !ejecutando ? 3 : 2));
            }

            attron(COLOR_PAIR(6));
            mvprintw(6, COLS / 2 + 3, "VmSize:        %llu KB",
                     metricas.virtual_memory_kb);
            attroff(COLOR_PAIR(6));
            attron(COLOR_PAIR(3) | A_BOLD);
            mvprintw(8, COLS / 2 + 3, "VmRSS:         %llu KB",
                     metricas.resident_memory_kb);
            attroff(COLOR_PAIR(3) | A_BOLD);
            attron(COLOR_PAIR(4));
            mvprintw(10, COLS / 2 + 3, "VmSwap:        %llu KB",
                     metricas.swap_kb);
            attroff(COLOR_PAIR(4));
            attron(COLOR_PAIR(4));
            mvprintw(12, COLS / 2 + 3, "Fallos pagina: %llu",
                     metricas.page_faults);
            attroff(COLOR_PAIR(4));

            attron(COLOR_PAIR(error ? 5 : ejecutando ? 4 : 3) | A_BOLD);
            mvprintw(LINES - 4, 3, "%s",
                     error ? mensaje.c_str() :
                     ejecutando ? "Prueba en ejecucion..." :
                     "Listo para iniciar");
            attroff(COLOR_PAIR(error ? 5 : ejecutando ? 4 : 3) | A_BOLD);
            attron(COLOR_PAIR(2));
            mvprintw(LINES - 2, 3, ejecutando
                     ? "[Q] Salir (libera memoria)"
                     : "[Flechas] Campo  [+/-] Ajustar  [E] Escribir  [ENTER] Ejecutar  [Q] Salir");
            attroff(COLOR_PAIR(2));
            refresh();

            int tecla = getch();
            if (tecla == 'q' || tecla == 'Q') {
                if (ejecutando && proceso_prueba > 0) {
                    kill(proceso_prueba, SIGTERM);
                    cerrarMonitor();
                }
                salir = true;
            }
            if (ejecutando) {
                MensajeMonitor recibido{};
                ssize_t leidos = 0;
                while ((leidos = read(descriptor_metricas, &recibido,
                                      sizeof(recibido))) == sizeof(recibido)) {
                    metricas = recibido.metricas;
                    if (recibido.estado != 0) {
                        error = true;
                        mensaje = recibido.mensaje;
                    }
                }
                int estado_proceso = 0;
                pid_t resultado_espera = waitpid(
                    proceso_prueba, &estado_proceso, WNOHANG);
                if (resultado_espera == proceso_prueba) {
                    if (WIFEXITED(estado_proceso) &&
                        WEXITSTATUS(estado_proceso) != 0 && !error) {
                        error = true;
                        mensaje = "La prueba termino con error";
                    } else if (!error) {
                        mensaje = "Prueba finalizada; memoria liberada";
                    }
                    close(descriptor_metricas);
                    descriptor_metricas = -1;
                    proceso_prueba = -1;
                    ejecutando = false;
                }
                continue;
            }
            if (tecla == KEY_UP && campo > 0) --campo;
            if (tecla == KEY_DOWN && campo < 4) ++campo;
            if (tecla == '+' || tecla == '=') {
                if (campo == 0) ++configuracion.memoria_mb;
                if (campo == 1) ++configuracion.limite_cpu_segundos;
                if (campo == 2) ++configuracion.limite_memoria_mb;
                if (campo == 3) ++configuracion.duracion_segundos;
                if (campo == 4) configuracion.intervalo_milisegundos += 50;
                error = false;
            }
            if (tecla == 'e' || tecla == 'E') {
                editarCampo();
            }
            if (tecla == '-' && valores[campo] > 1) {
                if (campo == 0) --configuracion.memoria_mb;
                if (campo == 1) --configuracion.limite_cpu_segundos;
                if (campo == 2) --configuracion.limite_memoria_mb;
                if (campo == 3) --configuracion.duracion_segundos;
                if (campo == 4 && configuracion.intervalo_milisegundos > 50)
                    configuracion.intervalo_milisegundos -= 50;
                error = false;
            }
            if (tecla == '\n' || tecla == KEY_ENTER) {
                int tuberia[2];
                if (pipe(tuberia) != 0) {
                    error = true;
                    mensaje = "No se pudo crear el monitor";
                    continue;
                }
                proceso_prueba = fork();
                if (proceso_prueba == 0) {
                    close(tuberia[0]);
                    MensajeMonitor salida{};
                    auto limites = aplicarLimitesRecursos(
                        configuracion.limite_cpu_segundos,
                        configuracion.limite_memoria_mb);
                    if (!limites.exito_cpu || !limites.exito_memoria) {
                        salida.estado = 1;
                        std::strncpy(salida.mensaje,
                                     limites.mensaje_error.c_str(),
                                     sizeof(salida.mensaje) - 1);
                        write(tuberia[1], &salida, sizeof(salida));
                        _exit(1);
                    }
                    auto reserva = iniciarPresionMemoria(
                        configuracion.memoria_mb);
                    if (!reserva.exito) {
                        salida.estado = 1;
                        std::strncpy(salida.mensaje,
                                     reserva.mensaje_error.c_str(),
                                     sizeof(salida.mensaje) - 1);
                        write(tuberia[1], &salida, sizeof(salida));
                        _exit(1);
                    }
                    const auto limite = std::chrono::steady_clock::now() +
                        std::chrono::seconds(configuracion.duracion_segundos);
                    while (std::chrono::steady_clock::now() < limite) {
                        salida.metricas = obtenerMetricasProceso();
                        write(tuberia[1], &salida, sizeof(salida));
                        std::this_thread::sleep_for(std::chrono::milliseconds(
                            configuracion.intervalo_milisegundos));
                    }
                    liberarPresionMemoria();
                    _exit(0);
                }
                close(tuberia[1]);
                int flags = fcntl(tuberia[0], F_GETFL, 0);
                fcntl(tuberia[0], F_SETFL, flags | O_NONBLOCK);
                descriptor_metricas = tuberia[0];
                error = false;
                ejecutando = true;
            }
        }

        if (ejecutando && proceso_prueba > 0) {
            kill(proceso_prueba, SIGTERM);
            cerrarMonitor();
        }
        liberarPresionMemoria();
        endwin();
        return 0;
    }
}
