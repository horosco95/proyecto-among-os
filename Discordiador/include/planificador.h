#ifndef _PLANIFICADOR_H_
#define _PLANIFICADOR_H_

#include <commons/collections/queue.h>
#include <stdbool.h>
#include "variables_globales_shared.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

// en ESTADO NEW: genera las estructuras administrativas , una vez generado todo --> pasa a READY.
t_queue *cola_new;
t_queue *cola_ready;
t_queue *cola_running;
t_queue *cola_bloqueado_io;
t_queue *cola_bloqueado_emergency;
t_queue *cola_exit;

//enum status_planificador estado_planificador;
// Estructura de dato para colas del planificador / dispatcher
typedef struct dato_tripulante{
    int PID, TID;
    int estado_previo;
    int quantum;
}t_tripulante;


// CODIGO ESTADO del Tripulante
enum estado_tripulante{
    NEW,
    READY,
    EXEC,
    BLOCKED_IO,
    BLOCKED_EMERGENCY,
    EXIT
};

enum status_planificador {
    PLANIFICADOR_OFF,
    PLANIFICADOR_RUNNING,
    PLANIFICADOR_BLOCKED
};

enum algoritmo {
    FIFO,
    RR
};

static void crear_colas();
int existe_tripulantes_en_cola(t_queue *cola);
static void pasar_todos_new_to_ready(enum algoritmo cod_algor);
static int transicion_new_to_ready(t_tripulante *dato, enum algoritmo cod_algor);
void listar_tripulantes(enum algoritmo cod);
static void imprimir_info_elemento_fifo(void *data);
static void imprimir_info_elemento_rr(void *data);
char* code_dispatcher_to_string(enum estado_tripulante code);

int hay_espacio_disponible(int grado_multiprocesamiento);
int hay_tarea_a_realizar(void);
int hay_tripulantes_a_borrar(void);
void expulsar_tripulante(void);
int hay_bloqueo_io(void);
int hay_sabotaje(void);
int se_solicito_lista_trip(void);
static void destructor_elementos_tripulante(void *data_tripulante);

void ordenar_lista_tid_ascendente(t_queue *listado);
static bool tripulante_tid_es_menor_que(void *data1, void *data2);
static void gestionar_bloqueo_io(void);
void bloquear_tripulantes_por_sabotaje(void);
static void desbloquear_tripulantes_tras_sabotaje(void);
int termino_sabotaje(void);
static enum algoritmo string_to_code_algor(char *string_code)

#endif