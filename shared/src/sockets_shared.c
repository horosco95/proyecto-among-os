#include "sockets_shared.h"

int recibir_operacion(int servidor_fd)
{
	int cod_op;
	if(recv(servidor_fd, &cod_op, sizeof(int), MSG_WAITALL) != 0)
		return cod_op;
	else
		return -1;
}

int recibir_payload_y_ejecutar(int socket_fd, int(*funcion_a_ejecutar)(char*)){
	int exit_status;
	char* payload = recibir_payload(socket_fd);
	exit_status = funcion_a_ejecutar(payload);
	free(payload);
	return exit_status;
}

int recibir_longitud_payload(int socket_emisor){
	int longitud_payload = 0;
	recv(socket_emisor, &longitud_payload, sizeof(int), MSG_WAITALL);
	return longitud_payload;
}

char* recibir_payload(int socket_emisor)
{
	int longitud_payload = recibir_longitud_payload(socket_emisor);
	char* payload = malloc(longitud_payload);
	recv(socket_emisor, payload, longitud_payload, MSG_WAITALL);
	return payload;
}

char* recibir_mensaje(int socket_cliente)
{
	char* mensaje = recibir_payload(socket_cliente);
	return mensaje;
}

int enviar_mensaje(int fd_destino, char* mensaje){	
	return enviar_operacion(fd_destino, COD_MENSAJE, mensaje, strlen(mensaje)+1);
}

int enviar_operacion(int fd_destino, int codigo_operacion, void* payload, int longitud_payload) {
	int bytes_enviados;
	int exit_status = EXIT_SUCCESS;
	char* stream;
	int offset = 0;
	int tamanio_stream;

	// Si no hay payload, solo se envia el codigo de operacion
	if(payload == NULL) {
		tamanio_stream = sizeof(codigo_operacion);

		stream = malloc(tamanio_stream);

		memcpy(stream + offset, &codigo_operacion, sizeof(int));
		offset += sizeof(int);
	}

	// Si hay payload, enviamos codigo de operacion + longitud + payload
	else {
		tamanio_stream = sizeof(codigo_operacion) + sizeof(longitud_payload) + longitud_payload;
	
		stream = malloc(tamanio_stream);
	
		memcpy(stream + offset, &codigo_operacion, sizeof(int));
		offset += sizeof(int);
		
		memcpy(stream + offset, &longitud_payload, sizeof(longitud_payload));
		offset += sizeof(int);

		memcpy(stream + offset, payload, longitud_payload);
		offset += sizeof(char) * longitud_payload;
	}

	bytes_enviados = send(fd_destino, stream, tamanio_stream, 0);

	if(bytes_enviados == -1){
		printf("ERROR. NO SE ENVIO LA OPERACION\n");
		exit_status = EXIT_FAILURE;
	}
	if(bytes_enviados < tamanio_stream){
		printf("ERROR. NO SE ENVIARON TODOS LOS BYTES\n");
		exit_status = EXIT_FAILURE;
	}

	free(stream);
	return exit_status;
}

int iniciar_servidor(char* ip, char* puerto)
{
	int socket_servidor;

    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(ip, puerto, &hints, &servinfo);

    for (p=servinfo; p != NULL; p = p->ai_next)
    {
        if ((socket_servidor = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (bind(socket_servidor, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_servidor);
            continue;
        }
        break;
    }

	listen(socket_servidor, SOMAXCONN);

    freeaddrinfo(servinfo);

    return socket_servidor;
}