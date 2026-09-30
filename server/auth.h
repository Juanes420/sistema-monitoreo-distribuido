#ifndef AUTH_H
#define AUTH_H

/* Verifica credenciales. Devuelve 1 si son válidas, 0 si no.
 * Si son válidas, copia el perfil (ADMIN / CONSULTA) en role_out. */
int auth_check(const char *username, const char *password, char *role_out);

#endif
