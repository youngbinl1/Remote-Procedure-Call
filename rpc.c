#include "rpc.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <assert.h>
#include <endian.h>

/******** Definitions of constants ********/
#define BEGINNING 0
#define REGISTERED 1
#define MISSING 3
#define RPC_CALL 9
#define RPC_FIND 7

///////////////// server API

struct rpc_server {
    int sockfd;
    int port;
    int num_handle;
    rpc_handle* serv_handle;
};

struct rpc_handle {
    char* name;
    int state;
    int location;
    rpc_handler handler;
};

/** initialise the rpc server **/ 
rpc_server* rpc_init_server(int port) {
    // create a new server
    rpc_server* server = (rpc_server*) malloc(sizeof(rpc_server));
    if (server == NULL) {
        return NULL;
    }

    int re;
    struct sockaddr_in6 serv_addr;

    // create TCP, ipv6 socket
    server->sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (server->sockfd == -1) {
        fprintf(stderr, "server socket creation error\n");
        exit(EXIT_FAILURE);
    }
    
    // Create address we're going to listen on
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin6_family = AF_INET6;  // IPv6
    serv_addr.sin6_addr = in6addr_any; // listen on any address
    serv_addr.sin6_port = htons(port); // port

	// Reuse port if possible
	re = 1;
	if (setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
		fprintf(stderr, "server setsockopt\n");
		exit(EXIT_FAILURE);
	}

	// Bind address to the socket
	if (bind(server->sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		fprintf(stderr, "server bind\n");
		exit(EXIT_FAILURE);
	}

    // after bind call
    int enable = 1;
    if (setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        fprintf(stderr, "serv setsockopt\n");
    }

    // assign initial values
    server->port = port;
    server->num_handle = BEGINNING;

    server->serv_handle = (rpc_handle*) malloc(sizeof(rpc_handle));
    server->serv_handle->name = (char*) malloc(sizeof(char));
    server->serv_handle->name = NULL;
    server->serv_handle->state = MISSING;
    server->serv_handle->handler = NULL;
    server->serv_handle->location = BEGINNING;

    // wait for connection
    if (listen(server->sockfd, 10) == -1) {
        fprintf(stderr, "serv listen error\n");
    }

    return server;
}

/** register a function with the function's name and handler **/ 
int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    // check if server or name is null
    if ((srv == NULL) || (name == NULL)){
        return -1;
    }

    int result = -1;
    int registered = MISSING;

    // look for the handle array to see if the name is already registered
    for (int i=0; i < srv->num_handle; i++) {
        if (strcmp(name, srv->serv_handle[i].name) == 0) {
            result = i;
            srv->serv_handle[i].location = i;
            registered = REGISTERED;
            // replace the old one with new handler
            srv->serv_handle[i].handler = handler;
            break;
        }
    }

    // there is no function registered with the same name
    if (registered == MISSING) {
        // register the function to server
        srv->serv_handle[srv->num_handle].name = (char*) malloc((strlen(name) + 1) * sizeof(char));
        strcpy(srv->serv_handle[srv->num_handle].name, name);
        
        srv->serv_handle[srv->num_handle].state = REGISTERED;
        srv->serv_handle[srv->num_handle].handler = handler;
        srv->serv_handle[srv->num_handle].location = srv->num_handle;
        result = srv->num_handle;
        srv->num_handle += 1;
    }

    return result;
}

/** function to be used for rpc_serve_all function when client requests rpc_find **/
void rpc_serve_all_for_find(int new_sock, rpc_server* srv) {

    ssize_t received_bytes1, received_bytes2;
    uint64_t length, converted_len;

    // read a data length of data2
    received_bytes1 = read(new_sock, &length, sizeof(length));
    if (received_bytes1 < 0) {
        fprintf(stderr, "read error serve_all\n");
    }
    // convert network byte order to host 
    converted_len = be64toh(length);

    // initialise data2 to receive data from client
    char* data2 = (char*)malloc(converted_len * sizeof(char));
    received_bytes2 = read(new_sock, data2, converted_len);
    if (received_bytes2 < 0) {
        fprintf(stderr, "read error serve_all\n");
    }
    
    // make a return value
    int result = MISSING;

    // look for the matching registered function
    for (int i=0; i < srv->num_handle; i++) {
        // registered function found
        if (strcmp(data2, srv->serv_handle[i].name) == 0) {
            result = REGISTERED;
            break;
        }
    }
    free(data2);
            
    // convert data to network byte order
    uint64_t data1_64;
    data1_64 = htobe64((uint64_t)result);
    // send it to client
    write(new_sock, &data1_64, sizeof(data1_64));
    return;
}

/** function takes requests from client and response it with proper data **/ 
void rpc_serve_all(rpc_server* srv) {
    // check if the server is null and return
    if (srv == NULL) {
        return;
    }

    int new_sock;
    struct sockaddr_in6 clnt_addr;
    socklen_t clnt_addr_size;
	// Accept a connection - blocks until a connection is ready to be accepted
	// Get back a new file descriptor to communicate on
    clnt_addr_size = sizeof(clnt_addr);
    new_sock = accept(srv->sockfd, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
    if (new_sock == -1) {
        fprintf(stderr, "server accept error\n");
        exit(EXIT_FAILURE);
    }

    uint64_t type, converted_type;
    ssize_t received_bytes1, received_bytes2, received_bytes3;

    // get request from client 
    while (1) {
        // read client call 
        received_bytes1 = read(new_sock, &type, sizeof(type));
        
        if (received_bytes1 > 0) {
            // convert it back to host
            converted_type = be64toh(type);

            // if the request is from rpc_find function
            if (converted_type == RPC_FIND) {
                rpc_serve_all_for_find(new_sock, srv);
            }
            // if the request is from rpc_call
            if (converted_type == RPC_CALL) {
                // read further information like the function's location and length of its name
                uint64_t location, name_len, converted_loc, converted_name_len;
                received_bytes2 = read(new_sock, &location, sizeof(location));
                received_bytes3 = read(new_sock, &name_len, sizeof(name_len));  

                if ((received_bytes2 < 0) || (received_bytes3 < 0)) {
                    fprintf(stderr, "read error in serve_all\n");
                }

                // convert it from network byte order
                converted_loc = be64toh(location);
                converted_name_len = be64toh(name_len);

                // initialise and read the name from client
                ssize_t received_bytes4, received_bytes5, received_bytes6, received_bytes7;
                char* name = (char*) malloc(converted_name_len * sizeof(char));
                received_bytes4 = read(new_sock, name, converted_name_len);
                if (received_bytes4 < 0) {
                    fprintf(stderr, "read error serve_all\n");
                }              
                // make a input data variable to put it to handler
                rpc_data* handler_input = (rpc_data*) malloc(sizeof(rpc_data));
                uint64_t data1, data2_len;
                
                // read necessary data and convert it for host
                // data1 value from payload
                received_bytes5 = read(new_sock, &data1, sizeof(data1));
                if (received_bytes5 < 0) {
                    fprintf(stderr, "read error serve_all\n");
                }
                handler_input->data1 = be64toh(data1);

                // data2_len from payload
                received_bytes6 = read(new_sock, &data2_len, sizeof(data2_len));
                if (received_bytes6 < 0) {
                    fprintf(stderr, "read error serve_all\n");
                }
                handler_input->data2_len = be64toh(data2_len);

                // data2 from payload in client
                handler_input->data2 = (char*)malloc(handler_input->data2_len * sizeof(char));
                received_bytes7 = read(new_sock, handler_input->data2, handler_input->data2_len);
                if (received_bytes7 < 0) {
                    fprintf(stderr, "read error serve_all\n");
                }
                // create and get a return value
                rpc_data* output = (rpc_data*) malloc(sizeof(rpc_data));
                output = srv->serv_handle[converted_loc].handler(handler_input);
                
                // convert and send data to client side
                uint64_t to_send_data1, to_send_data2_len;
                to_send_data1 = htobe64((uint64_t)(output->data1));           
                write(new_sock, &to_send_data1, sizeof(to_send_data1));
                to_send_data2_len = htobe64((uint64_t)(output->data2_len));
                write(new_sock, &to_send_data2_len, sizeof(to_send_data2_len));
                write(new_sock, output->data2, output->data2_len);
            }
        }
    }
    return;
}


///////////////// client API

struct rpc_client {
    int sockfd;
};

/** function to initialize the client **/ 
rpc_client *rpc_init_client(char *addr, int port) {

    // create a new client
    rpc_client* client = (rpc_client*) malloc(sizeof(rpc_client));
    if (client == NULL) {
        return NULL;
    }

    struct sockaddr_in6 serv_addr;

    // create TCP, ipv6 socket
    client->sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (client->sockfd == -1) {
        fprintf(stderr, "client socket creation error\n");
        exit(EXIT_FAILURE);
    }
    
    // Create address we're going to listen on
    memset(&serv_addr, 0, sizeof(serv_addr));
    
    serv_addr.sin6_family = AF_INET6;  // IPv6
    
    // get server information
    if (inet_pton(AF_INET6, addr, &(serv_addr.sin6_addr)) <= 0) {
        fprintf(stderr, "client got wrong address\n");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin6_port = htons(port); // port number

    // connect server to client socket
    int n;
    n = connect(client->sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (n == -1) {
        fprintf(stderr, "client connect error\n");
        return NULL;
    }

    return client;
}


// a function to search if a function is registered with its name and return its handle
rpc_handle* rpc_find(rpc_client *cl, char *name) {
    // either client or name missing
    if ((cl == NULL) || (name == NULL)) {
        return NULL;
    }

    // convert data to network byte order
    uint64_t type, name_len;
    type = htobe64((uint64_t)RPC_FIND);
    name_len = htobe64((uint64_t)(strlen(name) + 1));

    // send it to server
    write(cl->sockfd, &type, sizeof(type)); 
    write(cl->sockfd, &name_len, sizeof(name_len));
    write(cl->sockfd, name, strlen(name) + 1);    

    // read answer from server whether the function is registered
    uint64_t result, converted_result;
    ssize_t check1;
    check1 = read(cl->sockfd, &result, sizeof(result));
    if (check1 < 0) {
        fprintf(stderr, "read error in rpc_find\n");
    }
    // convert the given result
    converted_result = be64toh(result);

    // the function is registered, return handle
    if (converted_result == REGISTERED) {
        // create handle
        rpc_handle* output;
        output = (rpc_handle*) malloc(sizeof(rpc_handle));
        output->name = (char*) malloc((strlen(name) + 1) * sizeof(char));
        strcpy(output->name, name);
        output->state = REGISTERED;
        // client doesn't need to know handler and no information provided
        output->handler = NULL;
        return output;
    } 

    // couldn't find the function name or function not registered
    return NULL;
}

/** function to send input values for handler and get return value from server **/
rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    // return if any arguments are null
    if ((cl == NULL) || (h == NULL) || (payload == NULL)) {
        return NULL;
    }

    // let the server know rpc_call is about to send data
    uint64_t type = htobe64((uint64_t)RPC_CALL);
    write(cl->sockfd, &type, sizeof(type));

    // send information about the handle to the server
    uint64_t location, name_len;
    location = htobe64((uint64_t)(h->location));
    name_len = htobe64((uint64_t)(strlen(h->name) + 1));

    write(cl->sockfd, &location, sizeof(location));
    write(cl->sockfd, &name_len, sizeof(name_len));
    write(cl->sockfd, h->name, strlen(h->name) + 1);

    // send payload to server
    uint64_t data1, data2_len;
    data1 = htobe64((uint64_t)payload->data1);
    data2_len = htobe64((uint64_t)payload->data2_len);
    write(cl->sockfd, &data1, sizeof(data1));
    write(cl->sockfd, &data2_len, sizeof(data2_len));
    write(cl->sockfd, payload->data2, payload->data2_len);

    // receive answer from the server
    rpc_data* result = (rpc_data*) malloc(sizeof(rpc_data));
    read(cl->sockfd, &data1, sizeof(data1));
    result->data1 = be64toh((uint64_t)data1);

    read(cl->sockfd, &data2_len, sizeof(data2_len));
    result->data2_len = be64toh((uint64_t)data2_len);

    read(cl->sockfd, result->data2, result->data2_len);

    // fill in remained fields
    //result->data2 = NULL;
    //result->data2_len = 0;

    return result;
}

// function to close client 
void rpc_close_client(rpc_client *cl) {
    if (cl == NULL) {
        return;
    }
    close(cl->sockfd);
    free(cl);
}

// function to free rpc_data struct
void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);

    }
    free(data);
}
