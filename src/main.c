#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <getopt.h>
#include <csapp.h>

#include "debug.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"

#ifdef DEBUG
int _debug_packets_ = 1;
#endif

static void terminate(int status);
static void sighup_handler(int sig);
static void* thread(void* vargp);

static volatile sig_atomic_t sighup_received = 0;

/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    char* port = 0;

    int opt;
    while ((opt = getopt(argc, argv, "p:")) > -1) {
        switch(opt) {
            case 'p':
                if (optarg == NULL) {
                    //fprintf(stderr, "port number required, usage: -p <port>");
                    debug("port number required, usage: -p <port>");
                    return EXIT_FAILURE;
                }
                int port_number = atoi(optarg);
                if (port_number > 1024) {
                    port = optarg;
                } else {
                    //fprintf(stderr, "port numbers must be greater than 999");
                    debug("port numbers must be greater than 999");
                    return EXIT_FAILURE;
                }
                break;
            case '?':
                //fprintf(stderr, "invalid arguments\n");
                debug("invalid arguments\n");
                return EXIT_FAILURE;
            default:
                //fprintf(stderr, "invalid arguments\n");
                debug("invalid arguments\n");
                return EXIT_FAILURE;
        }
    }

    if (port == 0) {
        fprintf(stderr, "usage: -p <port>\n");
        return EXIT_FAILURE;
    }

    // Perform required initializations of the client_registry and
    // player_registry.
    client_registry = creg_init();
    player_registry = preg_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function jeux_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    struct sigaction sighup_action;
    sigemptyset(&sighup_action.sa_mask);
    sighup_action.sa_handler = sighup_handler;
    sighup_action.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sighup_action, NULL);

    int *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    int listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (SA*)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, jeux_client_service, connfdp);
    }

    fprintf(stderr, "You have to finish implementing main() "
	    "before the Jeux server will function.\n");

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    
    debug("%ld: Waiting for service threads to terminate...", pthread_self());
    creg_wait_for_empty(client_registry);
    debug("%ld: All service threads terminated.", pthread_self());

    // Finalize modules.
    creg_fini(client_registry);
    preg_fini(player_registry);

    debug("%ld: Jeux server terminating", pthread_self());
    exit(status);
}

void sighup_handler(int sig) {
    terminate(EXIT_SUCCESS);
}

/*void* thread(void* vargp) {
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    jeux_client_service(NULL);
    close(connfd);
    return NULL;
}*/
