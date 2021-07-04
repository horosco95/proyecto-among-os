#include "planificador.h"

void gestionar_tripulantes_en_exit();
int transicion(t_tripulante *tripulante, enum estado_tripulante estado_inicial, enum estado_tripulante estado_final);
bool existen_tripulantes_en_cola(t_queue *cola);
void pasar_todos_new_to_ready(enum algoritmo cod_algor);
 int hay_espacio_disponible(int grado_multiprocesamiento);
 void imprimir_info_elemento(void *data);
 void destructor_elementos_tripulante(void *data_tripulante);
 bool tripulante_tid_es_menor_que(void *lower, void *upper);
 int hay_sabotaje(void);
 int bloquear_tripulantes_por_sabotaje(void);
 int termino_sabotaje(void);
 void desbloquear_tripulantes_tras_sabotaje(void);
 int get_buffer_peticiones_and_swap_exec_blocked_io(t_queue *peticiones_origen, enum algoritmo code_algor);
 int get_buffer_peticiones_and_swap_blocked_io_ready(t_queue *peticiones_origen, enum algoritmo code_algor);
 void gestionar_exec(int grado_multiprocesamiento);
 int get_index_from_cola_by_tid(t_queue *src_list, int tid_buscado);
 t_tripulante *obtener_tripulante_por_tid(t_queue *cola_src, int tid_buscado);
 int hay_tripulantes_sin_quantum();
 void gestionar_tripulantes_sin_quantum();
 int hay_tarea_a_realizar(void);

int dispatcher(void *algor_planif){
/*
    int flag_espacio_new = 0;
    int espacio_disponible = 0;
    int hay_tarea = 0;
    int hay_trip_a_borrar = 0;
    int hay_block_io = 0;
    int hay_sabot = 0;
    int listado_de_tripulantes_solicitado = 0;
*/
    int code_algor = string_to_code_algor(algor_planif);
	
    while(true){

        //log_debug(logger, "loop consultas");
        // Se admiten en el sistema cada uno de los tripulantes creados
        sem_wait(&sem_mutex_ejecutar_dispatcher);
        sem_wait(&sem_mutex_ingreso_tripulantes_new);
		if( existen_tripulantes_en_cola(cola[NEW]) > 0){
            log_debug(logger, "Atendiendo COLA NEW");
            pasar_todos_new_to_ready(code_algor);
        }
        sem_post(&sem_mutex_ingreso_tripulantes_new);
        sem_post(&sem_mutex_ejecutar_dispatcher);
        
        //espacio_disponible = hay_espacio_disponible();
        //hay_tarea = hay_tarea_a_realizar();
        sem_wait(&sem_mutex_ejecutar_dispatcher);
		if( hay_espacio_disponible(grado_multiproc) && existen_tripulantes_en_cola(cola[READY]) ){
            log_debug(logger, "Atendiendo COLA READY");
            gestionar_exec(grado_multiproc);
		}
        sem_post(&sem_mutex_ejecutar_dispatcher);

        // Consulto si hay que atender Bloqueos IO
        sem_wait(&sem_mutex_ejecutar_dispatcher);
        if(existen_tripulantes_en_cola(buffer_peticiones_blocked_io_to_ready)){
            log_debug(logger, "Atendiendo COLA BLOCKED IO");
            get_buffer_peticiones_and_swap_blocked_io_ready(buffer_peticiones_blocked_io_to_ready, code_algor);

        }
        sem_post(&sem_mutex_ejecutar_dispatcher);

        sem_wait(&sem_mutex_ejecutar_dispatcher);
        if(existen_tripulantes_en_cola(buffer_peticiones_exec_to_blocked_io)){
            log_debug(logger, "Atendiendo COLA BLOCKED IO");
            get_buffer_peticiones_and_swap_exec_blocked_io(buffer_peticiones_exec_to_blocked_io, code_algor);
        }
        sem_post(&sem_mutex_ejecutar_dispatcher);

        // Consulto si hay sabotaje
        sem_wait(&sem_mutex_ejecutar_dispatcher);
		if( hay_sabotaje() ){
            //Atendiendo sabotaje
            log_debug(logger, "Atendiendo COLA BLOCKED_EMERGENCY");
            if (bloquear_tripulantes_por_sabotaje() != EXIT_SUCCESS)
                log_error(logger, "No se pudo ejecutar la funcion de bloqueo ante sabotajes");
            //while ( !termino_sabotaje() ); //ESPERA ACTIVA o implementar SEMÁFORO.
            //TODO: pasar TRIPULANTE que atendio SABOTAJE de EXEC a BLOCKED_IO
            log_debug(logger, "Termino sabotaje"); 
            desbloquear_tripulantes_tras_sabotaje();
		}
        sem_post(&sem_mutex_ejecutar_dispatcher);

        sem_wait(&sem_mutex_ejecutar_dispatcher);
		if( hay_tripulantes_sin_quantum() )
            gestionar_tripulantes_sin_quantum();
        sem_post(&sem_mutex_ejecutar_dispatcher);

        sem_wait(&sem_mutex_ejecutar_dispatcher);
		while( existen_tripulantes_en_cola(cola[EXIT]) )
            gestionar_tripulantes_en_exit();
        sem_post(&sem_mutex_ejecutar_dispatcher);
    }

	//libero recursos:
    log_error(logger, "Desarmando todos los tripulantes de las colas por finalizacion");
    for(int tipo_cola = 0;tipo_cola < CANT_COLAS; tipo_cola++){
        list_destroy_and_destroy_elements(cola[tipo_cola]->elements, destructor_elementos_tripulante);
        sem_destroy(mutex_cola[tipo_cola]);
        free(mutex_cola[tipo_cola]);
    }
    list_destroy_and_destroy_elements(lista_tripulantes, destructor_elementos_tripulante);
    free(cola);
    free(mutex_cola); 
    return 0;
}


void gestionar_tripulantes_en_exit(){

    log_error(logger, "Iniciando gestion de cola EXIT");

    // Saco el primer tripulante de la lista de exit
    t_tripulante *tripulante = queue_pop(cola[EXIT]);
    int tid = tripulante->TID;

    log_error(logger, "Iniciando expulsion tripulante %d", tripulante->TID);

    // Le aviso al tripulante que deje la cola de exit
    sem_post(tripulante->sem_tripulante_dejo[EXIT]);

    log_error(logger, "El tripulante %d dejo EXIT", tripulante->TID);

    // Espero a que el submodulo tripulante finalice para borrar sus estructuras
    sem_wait(tripulante->sem_finalizo);
    log_error(logger, "El submodulo tripulante %d finalizo", tripulante->TID);

    // Destruyo todas sus estructuras del Discordiador
    log_error(logger, "Destruyo estructuras del tripulante %d", tripulante->TID);
    destructor_elementos_tripulante(tripulante);

    log_debug(logger, "Se elimino al tripulante %d exitosamente", tid);
}

int iniciar_dispatcher(char *algoritmo_planificador){

    if(estado_planificador == PLANIFICADOR_OFF){
        pthread_t *hilo_dispatcher;
        hilo_dispatcher = malloc(sizeof(pthread_t));

        // Creando hilo del dispatcher
        pthread_create(hilo_dispatcher, NULL, (void*) dispatcher,&algoritmo_planificador);
        pthread_detach(*hilo_dispatcher);
        free(hilo_dispatcher);

        estado_planificador = PLANIFICADOR_RUNNING;
    }
    else if(estado_planificador == PLANIFICADOR_BLOCKED){
        sem_post(&sem_mutex_ejecutar_dispatcher);
        estado_planificador = PLANIFICADOR_RUNNING;
    }
    
    return EXIT_SUCCESS;
}

 void crear_colas(){
    cola = malloc(sizeof(t_queue *) * CANT_COLAS);
    mutex_cola = malloc(sizeof(sem_t *) * CANT_COLAS);
    for(int i = 0;i < CANT_COLAS;i++){
        cola[i] = queue_create();
        mutex_cola[i] = malloc(sizeof(sem_t));
        sem_init(mutex_cola[i], 0, 1);
    }
    
    /*
    buffer_peticiones_exec_to_blocked_io = queue_create();
    buffer_peticiones_blocked_io_to_ready = queue_create();
    buffer_peticiones_exec_to_ready = queue_create();
    */
}

 void pasar_todos_new_to_ready(enum algoritmo cod_algor){
    int cant_cola = queue_size(cola[NEW]);
    t_tripulante *transito;
    
    for (int i = cant_cola; i > 0; i--){
        transito = queue_pop(cola[NEW]); 
        transicion(transito, NEW, READY);
    }
}

int transicion(t_tripulante *tripulante, enum estado_tripulante estado_inicial, enum estado_tripulante estado_final){
    tripulante->estado = estado_final;
    tripulante->quantum = quantum;
    encolar(estado_final, tripulante);
    log_debug(logger, "Tripulante %d: %s => %s", 
        tripulante->TID, 
        code_dispatcher_to_string(estado_inicial),
        code_dispatcher_to_string(estado_final));
    sem_post(tripulante->sem_tripulante_dejo[estado_inicial]);
    return EXIT_SUCCESS;
}

bool existen_tripulantes_en_cola(t_queue *cola){
    return (queue_size(cola) > 0)? true: false;
}

 int hay_espacio_disponible(int grado_multiprocesamiento){
    return (queue_size(cola[EXEC]) < grado_multiprocesamiento)? 1: 0;
}

int hay_tarea_a_realizar(void){
    // TODO: fijarse si hay siguiente tarea a leer AUN. ¿POSIBLE sincronizacion?
    // USAR funciones de commons txt / o medir si no alcanzó a EOF de lista_tareas.txt
    return 1;
}

int rutina_expulsar_tripulante(void* args){
    int tid_buscado = *((int*) args);
    free(args);
    int estado = -1;
    t_tripulante *trip_expulsado;

    if ( estado_planificador == PLANIFICADOR_OFF ){
        log_error(logger,"ERROR. rutina_expulsar_tripulante: El planificador esta apagado");
        return -1;
    }

    sem_wait(&sem_mutex_ejecutar_dispatcher);

    for(int estado = 0; estado < CANT_ESTADOS; estado++){
        estado = get_index_from_cola_by_tid(cola[estado], tid_buscado);
        if(estado != -1){
            trip_expulsado = obtener_tripulante_por_tid(cola[estado], tid_buscado);
            transicion(trip_expulsado, estado, EXIT);
	        sem_post(&sem_mutex_ejecutar_dispatcher);
            return EXIT_SUCCESS;
        }
    }
	
    log_error(logger,"El tripulante %d no existe", tid_buscado);
	sem_post(&sem_mutex_ejecutar_dispatcher);
    return EXIT_FAILURE;
}

 int hay_sabotaje(void){
    // TODO: verificar cuando reciba el MODULO DISCORDIADOR el aviso de parte de iMongo-Store.
    return 0;
}

int listar_tripulantes(void){
    char *fecha = temporal_get_string_time("%d/%m/%y %H:%M:%S");
    printf("------------------------------------------------\n");
    printf("Estado de la Nave: %s\n",fecha);
    list_iterate(lista_tripulantes, imprimir_info_elemento);
    printf("------------------------------------------------\n");
    free(fecha);
    return EXIT_SUCCESS;
}

void imprimir_info_elemento(void *data){
    t_tripulante *tripulante = data;
    printf("Tripulante:%3d\tPatota:%3d\tStatus: %s\n", tripulante->TID, tripulante->PID, code_dispatcher_to_string(tripulante->estado));
}

void destructor_elementos_tripulante(void *data){
    t_tripulante *tripulante = (t_tripulante*) data;

    bool tiene_TID_a_eliminar(void* args){
        t_tripulante* tripulante_encontrado = (t_tripulante*) args;
        return tripulante_encontrado->TID == tripulante->TID;
    }

    // Lo quito de la lista global de tripulantes
    list_remove_by_condition(lista_tripulantes, tiene_TID_a_eliminar);

    // Destruyo todos sus semaforos
    sem_destroy(tripulante->sem_planificacion_fue_reanudada); 
    free(tripulante->sem_planificacion_fue_reanudada);   
    sem_destroy(tripulante->sem_finalizo); 
    free(tripulante->sem_finalizo);        

    for(int i = 0;i < CANT_COLAS;i++){
        sem_destroy(tripulante->sem_tripulante_dejo[i]); 
        free(tripulante->sem_tripulante_dejo[i]);
    }

    free(tripulante);   // Libero el struct
}

char *code_dispatcher_to_string(enum estado_tripulante code){
    switch(code){
        case NEW:
            return "NEW";
        case READY:
            return "READY";
        case EXEC:
            return "EXEC";
        case BLOCKED_IO:
            return "BLOCKED_IO";
        case BLOCKED_EMERGENCY:
            return "BLOCKED_EMERGENCY";
        case EXIT:
            return "EXIT";
        default:
            return "";
    }
}

int bloquear_tripulantes_por_sabotaje(void){

    //Pasar todos los tripulantes a blocked_emerg SEGUN ORDEN (1-EXEC -> 2-READY ->3- BLOCKED_IO)
    t_queue *temporal;
    temporal = queue_create();
    //TODO
    //OBS: el UNICO tripulante que debe permanecer en EXEC es quien TRABAJA/ATIENDE el SABOTAJE.

    // Pasar desde cola EXEC/RUNNING
    while(existen_tripulantes_en_cola(cola[EXEC]))
        list_add_sorted(cola[BLOCKED_EMERGENCY]->elements,queue_pop(cola[EXEC]),tripulante_tid_es_menor_que);

    // Pasar desde cola READY
    while(existen_tripulantes_en_cola(cola[READY])){
        list_add_sorted(temporal->elements,queue_pop(cola[READY]),tripulante_tid_es_menor_que);        
        encolar(BLOCKED_EMERGENCY, queue_pop(temporal));
    }

    // Pasar desde cola BLOCKED_IO
    while(existen_tripulantes_en_cola(cola[BLOCKED_IO])){
        list_add_sorted(temporal->elements,queue_pop(cola[BLOCKED_IO]),tripulante_tid_es_menor_que);
        encolar(BLOCKED_EMERGENCY, queue_pop(temporal));
    }

    queue_destroy(temporal);

    if (!existen_tripulantes_en_cola(cola[BLOCKED_EMERGENCY]))
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

 int termino_sabotaje(void){
    // TODO: verificar si termina el sabotaje. ¿Implementar con flag?
    return 0;
}

 bool tripulante_tid_es_menor_que(void *data1, void *data2){
    t_tripulante *temp1;
    t_tripulante *temp2;
    temp1 = data1;
    temp2 = data2;
    return (temp1->TID <= temp2->TID)? true: false;
}

 int get_index_from_cola_by_tid(t_queue *src_list, int tid_buscado){
    t_list_iterator *iterador_nuevo;
    iterador_nuevo = list_iterator_create(src_list->elements);
    t_tripulante *tripulante_buscado;
    int index_buscado = -1;

    // Busqueda del tripulante por TID y retornar su index
    while( list_iterator_has_next(iterador_nuevo) ){
        tripulante_buscado = list_iterator_next(iterador_nuevo);
        if(tripulante_buscado->TID == tid_buscado){
            index_buscado = iterador_nuevo->index;
            break;
        }
    }
    list_iterator_destroy(iterador_nuevo);
    return index_buscado;
}

// Revisar (no van todos a ready)
 void desbloquear_tripulantes_tras_sabotaje(void){
    while(existen_tripulantes_en_cola(cola[BLOCKED_EMERGENCY])){
        encolar(READY, queue_pop(cola[BLOCKED_EMERGENCY]));
    }
}

enum algoritmo string_to_code_algor(char *string_code){
    if (strcmp("RR",string_code) == 0)
        return RR;
    else
        return FIFO;
}// TODO: evaluar su aplicacion

int get_buffer_peticiones_and_swap_exec_blocked_io(t_queue *peticiones_origen, enum algoritmo code_algor){
    t_tripulante *tripulante;
    if ( queue_size(peticiones_origen) > 0){
        while( queue_size(peticiones_origen) > 0 ){
            tripulante = queue_pop(peticiones_origen);
            obtener_tripulante_por_tid(cola[EXEC], tripulante->TID);
            transicion(tripulante, EXEC, BLOCKED_IO);
        }
        return EXIT_SUCCESS;
    }
    else
        return EXIT_FAILURE;
}

 int get_buffer_peticiones_and_swap_blocked_io_ready(t_queue *peticiones_origen, enum algoritmo code_algor){
    t_tripulante *tripulante;
    if ( queue_size(peticiones_origen) > 0){
        while( queue_size(peticiones_origen) > 0 ){
            tripulante = queue_pop(peticiones_origen);
            obtener_tripulante_por_tid(cola[BLOCKED_IO], tripulante->TID);
            transicion(tripulante, BLOCKED_IO, READY);
        }
        return EXIT_SUCCESS;
    }else
        return EXIT_FAILURE;
}

 void gestionar_exec(int grado_multiprocesamiento){
    t_tripulante *temp;
    while( queue_size(cola[EXEC]) < grado_multiprocesamiento && existen_tripulantes_en_cola(cola[READY]) ){
        temp = queue_pop(cola[READY]);
        transicion(temp, READY, EXEC);
    }
}

void dispatcher_pausar(){
    estado_planificador = PLANIFICADOR_BLOCKED;
    sem_wait(&sem_mutex_ejecutar_dispatcher);
}

t_tripulante *obtener_tripulante_por_tid(t_queue *cola_src, int tid_buscado){
    int index;
    index = get_index_from_cola_by_tid(cola_src, tid_buscado);
    return list_remove(cola_src->elements, index);
}

int hay_tripulantes_sin_quantum(){
    return existen_tripulantes_en_cola(buffer_peticiones_exec_to_ready);
}

void gestionar_tripulantes_sin_quantum(){
    int *buffer;
    while( hay_tripulantes_sin_quantum() ){
        buffer = queue_pop(buffer_peticiones_exec_to_ready);
        transicion(obtener_tripulante_por_tid(cola[EXEC], *buffer), EXEC, READY);
    }
}

t_tripulante* iniciador_tripulante(int tid, int pid){
    t_tripulante *nuevo;
    nuevo = malloc(sizeof(t_tripulante));
    nuevo -> PID = pid;
    nuevo -> TID = tid;
    nuevo -> estado = NEW;
    
	// Inicializamos los semaforos (inicializan todos en 0)
    nuevo -> sem_planificacion_fue_reanudada = malloc(sizeof(sem_t));
	sem_init(nuevo -> sem_planificacion_fue_reanudada, 0, 0);    
    nuevo -> sem_finalizo = malloc(sizeof(sem_t));
	sem_init(nuevo -> sem_finalizo, 0, 0);    

    nuevo -> sem_tripulante_dejo = malloc(sizeof(sem_t*) * 6);
    for(int i = 0;i < CANT_ESTADOS;i++){
        nuevo->sem_tripulante_dejo[i] = malloc(sizeof(sem_t));
        sem_init(nuevo->sem_tripulante_dejo[i], 0, 0);
    }

    // Lo agrego a la lista global de tripulantes ordenado por TID
    list_add_sorted(lista_tripulantes, nuevo, tripulante_tid_es_menor_que);
    queue_push(cola[NEW], nuevo);
    return nuevo;
}

void encolar(int tipo_cola, t_tripulante* tripulante){
    sem_wait(mutex_cola[tipo_cola]);
    queue_push(cola[tipo_cola], tripulante);
    sem_post(mutex_cola[tipo_cola]); 
}