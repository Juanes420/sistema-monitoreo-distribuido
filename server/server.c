#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "protocol.h"
#include "auth.h"

/* ==================== Configuración ==================== */
#define MAX_NODES 10
#define MAX_HIST 5
#define NODE_TIMEOUT_SECONDS 15
#define BACKLOG 16

/* ==================== Estado global ==================== */
typedef struct {
    double ram;
    double temp;
    time_t ts;
} HistEntry;

typedef struct {
    int used;
    char id[MAX_FIELD];
    int registered;
    double ram;
    double temp;
    time_t last_update;
    int active; /* 1 = Activo, 0 = Inactivo */
    HistEntry hist[MAX_HIST];
    int hist_count;
    int hist_next;
} Node;

static Node g_nodes[MAX_NODES];
static int g_node_count = 0;
static pthread_mutex_t g_nodes_mutex = PTHREAD_MUTEX_INITIALIZER;

static FILE *g_logfile = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static int g_udp_port = 0; /* usamos el mismo número de puerto para TCP y UDP */
static long g_msg_counter = 0;
static pthread_mutex_t g_counter_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ==================== Utilidades ==================== */

static void now_iso8601(char *out, size_t outsize) {
    time_t t = time(NULL);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    strftime(out, outsize, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

static long next_msg_id(void) {
    long id;
    pthread_mutex_lock(&g_counter_mutex);
    id = ++g_msg_counter;
    pthread_mutex_unlock(&g_counter_mutex);
    return id;
}

/* Log a consola y a archivo, incluyendo IP:puerto del cliente */
static void log_line(const char *client_ip, int client_port, const char *direction, const char *content) {
    char ts[64];
    now_iso8601(ts, sizeof(ts));

    pthread_mutex_lock(&g_log_mutex);
    printf("[%s] %s %s:%d -> %s\n", ts, direction, client_ip, client_port, content);
    fflush(stdout);
    if (g_logfile) {
        fprintf(g_logfile, "[%s] %s %s:%d -> %s\n", ts, direction, client_ip, client_port, content);
        fflush(g_logfile);
    }
    pthread_mutex_unlock(&g_log_mutex);
}

/* Intenta resolver el nombre de dominio del cliente (requerimiento 4).
 * Si falla, se maneja la excepción sin afectar la ejecución del servidor. */
static void resolve_client_name(struct sockaddr_in *addr, char *out, size_t outsize) {
    char host[NI_MAXHOST];
    int rc = getnameinfo((struct sockaddr *)addr, sizeof(*addr), host, sizeof(host), NULL, 0, NI_NAMEREQD);
    if (rc != 0) {
        /* No se pudo resolver: se usa la IP tal cual, no se termina el servidor */
        snprintf(out, outsize, "%s", inet_ntoa(addr->sin_addr));
    } else {
        snprintf(out, outsize, "%s", host);
    }
}

/* Parsea un mensaje SRMP: TIPO|ID_ORIGEN|ID_MENSAJE|TIMESTAMP|LONGITUD|PAYLOAD */
static int parse_msg(const char *raw, SrmpMsg *m) {
    memset(m, 0, sizeof(*m));
    char buf[MAX_MSG];
    strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    char *rest = buf;
    char *tok;

    tok = strsep(&rest, "|"); if (!tok) return -1; strncpy(m->tipo, tok, MAX_FIELD - 1);
    tok = strsep(&rest, "|"); if (!tok) return -1; strncpy(m->id_origen, tok, MAX_FIELD - 1);
    tok = strsep(&rest, "|"); if (!tok) return -1; strncpy(m->id_mensaje, tok, MAX_FIELD - 1);
    tok = strsep(&rest, "|"); if (!tok) return -1; strncpy(m->timestamp, tok, MAX_FIELD - 1);
    tok = strsep(&rest, "|"); if (!tok) return -1; m->longitud = atoi(tok);

    if (rest) {
        strncpy(m->payload, rest, MAX_PAYLOAD - 1);
    } else {
        m->payload[0] = 0;
    }
    return 0;
}

/* Construye un mensaje SRMP saliente */
static void build_msg(char *out, size_t outsize, const char *tipo, const char *payload) {
    char ts[64];
    now_iso8601(ts, sizeof(ts));
    long id = next_msg_id();
    snprintf(out, outsize, "%s|servidor|%ld|%s|%d|%s\n",
             tipo, id, ts, (int)strlen(payload), payload);
}

/* ==================== Lógica de nodos ==================== */

/* Busca un nodo por id. Debe llamarse con g_nodes_mutex ya tomado. */
static Node *find_node_locked(const char *id) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (g_nodes[i].used && strcmp(g_nodes[i].id, id) == 0) {
            return &g_nodes[i];
        }
    }
    return NULL;
}

static Node *register_node(const char *id) {
    pthread_mutex_lock(&g_nodes_mutex);
    Node *n = find_node_locked(id);
    if (!n) {
        for (int i = 0; i < MAX_NODES; i++) {
            if (!g_nodes[i].used) {
                n = &g_nodes[i];
                memset(n, 0, sizeof(*n));
                n->used = 1;
                strncpy(n->id, id, MAX_FIELD - 1);
                g_node_count++;
                break;
            }
        }
    }
    if (n) {
        n->registered = 1;
        n->last_update = time(NULL);
        n->active = 1;
    }
    pthread_mutex_unlock(&g_nodes_mutex);
    return n;
}

static void update_node_metrics(const char *id, double ram, double temp) {
    pthread_mutex_lock(&g_nodes_mutex);
    Node *n = find_node_locked(id);
    if (n && n->registered) {
        int was_inactive = !n->active;
        n->ram = ram;
        n->temp = temp;
        n->last_update = time(NULL);
        n->active = 1;

        HistEntry *h = &n->hist[n->hist_next];
        h->ram = ram;
        h->temp = temp;
        h->ts = n->last_update;
        n->hist_next = (n->hist_next + 1) % MAX_HIST;
        if (n->hist_count < MAX_HIST) n->hist_count++;

        if (was_inactive) {
            log_line("interno", 0, "EVENTO", "Nodo reconectado tras estar Inactivo");
        }
    }
    pthread_mutex_unlock(&g_nodes_mutex);
}

/* Hilo que marca nodos como Inactivo si dejan de reportar ESTADO */
static void *timeout_checker(void *arg) {
    (void)arg;
    while (1) {
        sleep(5);
        time_t now = time(NULL);
        pthread_mutex_lock(&g_nodes_mutex);
        for (int i = 0; i < MAX_NODES; i++) {
            if (g_nodes[i].used && g_nodes[i].registered && g_nodes[i].active) {
                if (now - g_nodes[i].last_update > NODE_TIMEOUT_SECONDS) {
                    g_nodes[i].active = 0;
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Nodo %s marcado como Inactivo (timeout)", g_nodes[i].id);
                    log_line("interno", 0, "EVENTO", msg);
                }
            }
        }
        pthread_mutex_unlock(&g_nodes_mutex);
    }
    return NULL;
}

/* ==================== Manejo de mensajes ==================== */

static void handle_reg_nodo(const SrmpMsg *m, char *resp, size_t respsize) {
    register_node(m->id_origen);
    build_msg(resp, respsize, MSG_ACK, "OK");
}

static void handle_estado_common(const SrmpMsg *m) {
    double ram = 0, temp = 0;
    if (sscanf(m->payload, "RAM=%lf;TEMP=%lf", &ram, &temp) == 2) {
        update_node_metrics(m->id_origen, ram, temp);
    }
}

static void handle_evento(const SrmpMsg *m, char *resp, size_t respsize) {
    pthread_mutex_lock(&g_nodes_mutex);
    Node *n = find_node_locked(m->id_origen);
    pthread_mutex_unlock(&g_nodes_mutex);

    if (!n || !n->registered) {
        build_msg(resp, respsize, MSG_ERROR, "NODO_NO_REGISTRADO");
        return;
    }
    char msg[256];
    snprintf(msg, sizeof(msg), "EVENTO de %s: %s", m->id_origen, m->payload);
    log_line("interno", 0, "EVENTO", msg);
    build_msg(resp, respsize, MSG_ACK, "OK");
}

static void handle_login(const SrmpMsg *m, char *resp, size_t respsize, int *authenticated, char *role_out) {
    char user[128] = "", pass[128] = "";
    sscanf(m->payload, "user=%127[^;];pass=%127[^;\n]", user, pass);

    char role[64] = "";
    if (auth_check(user, pass, role)) {
        *authenticated = 1;
        strncpy(role_out, role, 63);
        build_msg(resp, respsize, MSG_ACK, role);
    } else {
        *authenticated = 0;
        build_msg(resp, respsize, MSG_ERROR, "CREDENCIALES_INVALIDAS");
    }
}

static void handle_consulta_actual(const SrmpMsg *m, char *resp, size_t respsize) {
    char nodo[MAX_FIELD] = "";
    sscanf(m->payload, "nodo=%255[^;\n]", nodo);

    char payload[MAX_PAYLOAD] = "";
    pthread_mutex_lock(&g_nodes_mutex);
    for (int i = 0; i < MAX_NODES; i++) {
        if (!g_nodes[i].used) continue;
        if (strcmp(nodo, "ALL") != 0 && strcmp(nodo, g_nodes[i].id) != 0) continue;

        char entry[160];
        snprintf(entry, sizeof(entry), "%s,RAM=%.1f,TEMP=%.1f,ESTADO=%s;",
                 g_nodes[i].id, g_nodes[i].ram, g_nodes[i].temp,
                 g_nodes[i].active ? "ACTIVO" : "INACTIVO");
        strncat(payload, entry, sizeof(payload) - strlen(payload) - 1);
    }
    pthread_mutex_unlock(&g_nodes_mutex);

    if (payload[0] == 0) {
        build_msg(resp, respsize, MSG_ERROR, "NODO_NO_ENCONTRADO");
    } else {
        build_msg(resp, respsize, MSG_RESP_ESTADO, payload);
    }
}

static void handle_consulta_hist(const SrmpMsg *m, char *resp, size_t respsize) {
    char nodo[MAX_FIELD] = "";
    sscanf(m->payload, "nodo=%255[^;\n]", nodo);

    char payload[MAX_PAYLOAD] = "";
    pthread_mutex_lock(&g_nodes_mutex);
    Node *n = find_node_locked(nodo);
    if (n) {
        for (int i = 0; i < n->hist_count; i++) {
            int idx = (n->hist_next - n->hist_count + i + MAX_HIST) % MAX_HIST;
            char entry[96];
            char ts[64];
            struct tm tmv;
            gmtime_r(&n->hist[idx].ts, &tmv);
            strftime(ts, sizeof(ts), "%H:%M:%S", &tmv);
            snprintf(entry, sizeof(entry), "%s,RAM=%.1f,TEMP=%.1f;",
                     ts, n->hist[idx].ram, n->hist[idx].temp);
            strncat(payload, entry, sizeof(payload) - strlen(payload) - 1);
        }
    }
    pthread_mutex_unlock(&g_nodes_mutex);

    if (!n) {
        build_msg(resp, respsize, MSG_ERROR, "NODO_NO_REGISTRADO");
    } else if (payload[0] == 0) {
        build_msg(resp, respsize, MSG_ERROR, "SIN_HISTORICO");
    } else {
        build_msg(resp, respsize, MSG_RESP_HIST, payload);
    }
}

/* Procesa un mensaje ya parseado y genera la respuesta correspondiente */
static void dispatch(const SrmpMsg *m, char *resp, size_t respsize, int *authenticated, char *role) {
    if (strcmp(m->tipo, MSG_REG_NODO) == 0) {
        handle_reg_nodo(m, resp, respsize);
    } else if (strcmp(m->tipo, MSG_EVENTO) == 0) {
        handle_evento(m, resp, respsize);
    } else if (strcmp(m->tipo, MSG_LOGIN) == 0) {
        handle_login(m, resp, respsize, authenticated, role);
    } else if (strcmp(m->tipo, MSG_CONSULTA_ACTUAL) == 0) {
        if (!*authenticated) {
            build_msg(resp, respsize, MSG_ERROR, "NO_AUTENTICADO");
        } else {
            handle_consulta_actual(m, resp, respsize);
        }
    } else if (strcmp(m->tipo, MSG_CONSULTA_HIST) == 0) {
        if (!*authenticated) {
            build_msg(resp, respsize, MSG_ERROR, "NO_AUTENTICADO");
        } else {
            handle_consulta_hist(m, resp, respsize);
        }
    } else {
        build_msg(resp, respsize, MSG_ERROR, "TIPO_MENSAJE_DESCONOCIDO");
    }
}

/* ==================== Hilo TCP por cliente ==================== */

typedef struct {
    int fd;
    struct sockaddr_in addr;
} ClientArgs;

static void *handle_tcp_client(void *arg) {
    ClientArgs *ca = (ClientArgs *)arg;
    int fd = ca->fd;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ca->addr.sin_addr, ip, sizeof(ip));
    int port = ntohs(ca->addr.sin_port);

    char client_name[NI_MAXHOST];
    resolve_client_name(&ca->addr, client_name, sizeof(client_name));

    int authenticated = 0;
    char role[64] = "";

    char buf[MAX_MSG];
    size_t buflen = 0;

    while (1) {
        /* Lee hasta encontrar '\n' o hasta que el cliente cierre la conexión */
        ssize_t n = recv(fd, buf + buflen, sizeof(buf) - buflen - 1, 0);
        if (n <= 0) {
            break; /* desconexión o error de red */
        }
        buflen += n;
        buf[buflen] = 0;

        char *newline;
        while ((newline = strchr(buf, '\n')) != NULL) {
            *newline = 0;
            char raw_line[MAX_MSG];
            strncpy(raw_line, buf, sizeof(raw_line) - 1);

            log_line(ip, port, "IN", raw_line);

            SrmpMsg m;
            char resp[MAX_MSG];
            if (parse_msg(raw_line, &m) != 0) {
                build_msg(resp, sizeof(resp), MSG_ERROR, "FORMATO_INVALIDO");
            } else {
                dispatch(&m, resp, sizeof(resp), &authenticated, role);
            }

            if (send(fd, resp, strlen(resp), 0) < 0) {
                /* El cliente pudo haberse desconectado justo al responder;
                 * se ignora SIGPIPE (ver main) y simplemente se cierra el hilo. */
                close(fd);
                free(ca);
                return NULL;
            }
            log_line(ip, port, "OUT", resp);

            /* mover el resto del buffer al inicio */
            size_t consumed = (newline - buf) + 1;
            memmove(buf, buf + consumed, buflen - consumed);
            buflen -= consumed;
            buf[buflen] = 0;
        }

        if (buflen >= sizeof(buf) - 1) {
            /* mensaje demasiado largo sin salto de línea: se descarta */
            buflen = 0;
        }
    }

    close(fd);
    free(ca);
    return NULL;
}

/* ==================== Hilo UDP (telemetría ESTADO) ==================== */

static void *udp_loop(void *arg) {
    int fd = *(int *)arg;
    char buf[MAX_MSG];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    while (1) {
        ssize_t n = recvfrom(fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &fromlen);
        if (n <= 0) continue;
        buf[n] = 0;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
        int port = ntohs(from.sin_port);

        char clean[MAX_MSG];
        strncpy(clean, buf, sizeof(clean) - 1);
        clean[strcspn(clean, "\r\n")] = 0;

        log_line(ip, port, "IN(UDP)", clean);

        SrmpMsg m;
        if (parse_msg(clean, &m) == 0 && strcmp(m.tipo, MSG_ESTADO) == 0) {
            handle_estado_common(&m);
        }
        /* ESTADO no requiere ACK obligatorio, según el diseño de Fase 1 */
    }
    return NULL;
}

/* ==================== main ==================== */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <puerto> <archivoDeLogs>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    g_udp_port = port;
    g_logfile = fopen(argv[2], "a");
    if (!g_logfile) {
        fprintf(stderr, "No se pudo abrir el archivo de logs '%s': %s\n", argv[2], strerror(errno));
        /* No se termina la ejecución: se continúa solo con log en consola */
    }

    signal(SIGPIPE, SIG_IGN); /* evita que el servidor muera si un cliente cierra abruptamente */

    /* ---- Socket UDP ---- */
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) { perror("socket UDP"); return 1; }

    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(port);

    if (bind(udp_fd, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("bind UDP");
        return 1;
    }

    pthread_t udp_thread;
    pthread_create(&udp_thread, NULL, udp_loop, &udp_fd);
    pthread_detach(udp_thread);

    /* ---- Hilo de verificación de timeouts ---- */
    pthread_t timeout_thread;
    pthread_create(&timeout_thread, NULL, timeout_checker, NULL);
    pthread_detach(timeout_thread);

    /* ---- Socket TCP ---- */
    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd < 0) { perror("socket TCP"); return 1; }

    int opt = 1;
    setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in tcp_addr;
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(port);

    if (bind(tcp_fd, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("bind TCP");
        return 1;
    }

    if (listen(tcp_fd, BACKLOG) < 0) {
        perror("listen");
        return 1;
    }

    printf("Servidor SRMP escuchando en el puerto %d (TCP y UDP)\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(tcp_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue; /* un error puntual de accept no debe tumbar el servidor */
        }

        ClientArgs *ca = malloc(sizeof(ClientArgs));
        ca->fd = client_fd;
        ca->addr = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_tcp_client, ca) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(ca);
            continue;
        }
        pthread_detach(tid);
    }

    close(tcp_fd);
    close(udp_fd);
    if (g_logfile) fclose(g_logfile);
    return 0;
}
