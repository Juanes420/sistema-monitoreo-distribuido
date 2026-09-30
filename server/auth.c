#include <stdio.h>
#include <string.h>
#include "auth.h"

/*
 * NOTA IMPORTANTE (documentar en la sustentación):
 * Por restricciones de tiempo, este módulo valida credenciales contra un
 * archivo de texto plano (users.txt), pero está deliberadamente separado
 * de la lógica principal del servidor (server.c) en su propio módulo,
 * tal como recomienda el enunciado. En una siguiente iteración del
 * proyecto, este módulo podría reemplazarse fácilmente por una llamada a
 * un servicio de autenticación externo (por ejemplo, un microservicio
 * HTTP o LDAP) sin tener que tocar server.c, ya que auth_check() es la
 * única función que server.c conoce de este módulo.
 */

#define USERS_FILE "users.txt"
#define MAX_LINE 128

int auth_check(const char *username, const char *password, char *role_out) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) {
        fprintf(stderr, "[AUTH] No se pudo abrir %s\n", USERS_FILE);
        return 0;
    }

    char line[MAX_LINE];
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        /* formato: usuario:password:rol */
        char file_user[MAX_LINE], file_pass[MAX_LINE], file_role[MAX_LINE];
        line[strcspn(line, "\r\n")] = 0; /* quitar salto de linea */
        if (sscanf(line, "%127[^:]:%127[^:]:%127[^:\n]", file_user, file_pass, file_role) == 3) {
            if (strcmp(file_user, username) == 0 && strcmp(file_pass, password) == 0) {
                strncpy(role_out, file_role, MAX_LINE - 1);
                found = 1;
                break;
            }
        }
    }

    fclose(f);
    return found;
}
