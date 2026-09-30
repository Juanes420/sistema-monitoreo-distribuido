#!/usr/bin/env python3
"""
Cliente de administracion SRMP (Server Resource Monitoring Protocol)

Se autentica ante el servidor central y permite consultar el estado
actual e historico de los nodos monitoreados.

Uso:
    python3 client.py --host <host_servidor> --port <puerto>
"""

import argparse
import socket
import itertools
import getpass
from datetime import datetime, timezone

_msg_counter = itertools.count(1)


def timestamp():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def build_msg(tipo, id_origen, payload):
    msg_id = next(_msg_counter)
    ts = timestamp()
    return f"{tipo}|{id_origen}|{msg_id}|{ts}|{len(payload)}|{payload}"


def resolve_host(hostname):
    """Resuelve el nombre de dominio del servidor (requerimiento 4).
    Si falla, se informa el error sin terminar el programa abruptamente."""
    try:
        infos = socket.getaddrinfo(hostname, None, socket.AF_INET)
        return infos[0][4][0]
    except socket.gaierror as e:
        print(f"[ERROR] No se pudo resolver el nombre '{hostname}': {e}")
        return None


def parse_resp(resp):
    """Separa un mensaje SRMP en sus 6 campos."""
    partes = resp.split("|", 5)
    if len(partes) < 6:
        return None
    tipo, id_origen, id_msg, ts, longitud, payload = partes
    return {"tipo": tipo, "payload": payload}


def imprimir_estado(payload):
    entradas = [e for e in payload.split(";") if e]
    print(f"\n{'Nodo':<12}{'RAM %':<10}{'TEMP C':<10}{'Estado':<10}")
    print("-" * 42)
    for e in entradas:
        campos = e.split(",")
        nodo = campos[0]
        ram = campos[1].split("=")[1] if len(campos) > 1 else "?"
        temp = campos[2].split("=")[1] if len(campos) > 2 else "?"
        estado = campos[3].split("=")[1] if len(campos) > 3 else "?"
        print(f"{nodo:<12}{ram:<10}{temp:<10}{estado:<10}")
    print()


def imprimir_historico(payload):
    entradas = [e for e in payload.split(";") if e]
    print(f"\n{'Hora':<12}{'RAM %':<10}{'TEMP C':<10}")
    print("-" * 32)
    for e in entradas:
        campos = e.split(",")
        hora = campos[0]
        ram = campos[1].split("=")[1] if len(campos) > 1 else "?"
        temp = campos[2].split("=")[1] if len(campos) > 2 else "?"
        print(f"{hora:<12}{ram:<10}{temp:<10}")
    print()


class Cliente:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.sock = None
        self.id = "cliente01"

    def conectar(self):
        ip = resolve_host(self.host)
        if ip is None:
            return False
        try:
            self.sock = socket.create_connection((ip, self.port), timeout=5)
            return True
        except (ConnectionRefusedError, socket.timeout, OSError) as e:
            print(f"[ERROR] No se pudo conectar al servidor {self.host}:{self.port} -> {e}")
            return False

    def enviar(self, msg):
        try:
            self.sock.sendall((msg + "\n").encode())
            data = self.sock.recv(4096).decode().strip()
            return data
        except (socket.timeout, OSError) as e:
            print(f"[ERROR] Fallo de comunicacion con el servidor: {e}")
            return None

    def login(self):
        user = input("Usuario: ")
        pw = getpass.getpass("Contraseña: ")
        payload = f"user={user};pass={pw}"
        msg = build_msg("LOGIN", self.id, payload)
        resp = self.enviar(msg)
        if not resp:
            return False
        parsed = parse_resp(resp)
        if parsed and parsed["tipo"] == "ACK":
            print(f"[OK] Autenticado como perfil: {parsed['payload']}")
            return True
        else:
            print(f"[ERROR] {parsed['payload'] if parsed else resp}")
            return False

    def consulta_actual(self, nodo):
        payload = f"nodo={nodo}"
        msg = build_msg("CONSULTA_ACTUAL", self.id, payload)
        resp = self.enviar(msg)
        if not resp:
            return
        parsed = parse_resp(resp)
        if parsed and parsed["tipo"] == "RESP_ESTADO":
            imprimir_estado(parsed["payload"])
        else:
            print(f"[ERROR] {parsed['payload'] if parsed else resp}")

    def consulta_hist(self, nodo):
        payload = f"nodo={nodo}"
        msg = build_msg("CONSULTA_HIST", self.id, payload)
        resp = self.enviar(msg)
        if not resp:
            return
        parsed = parse_resp(resp)
        if parsed and parsed["tipo"] == "RESP_HIST":
            imprimir_historico(parsed["payload"])
        else:
            print(f"[ERROR] {parsed['payload'] if parsed else resp}")


def menu():
    print("\n=== Cliente de administracion SRMP ===")
    print("1) Consultar estado actual de TODOS los nodos")
    print("2) Consultar estado actual de un nodo especifico")
    print("3) Consultar historico de un nodo")
    print("4) Salir")
    return input("Seleccione una opcion: ").strip()


def main():
    parser = argparse.ArgumentParser(description="Cliente de administracion SRMP")
    parser.add_argument("--host", required=True, help="Nombre de dominio o host del servidor")
    parser.add_argument("--port", type=int, required=True, help="Puerto del servidor")
    args = parser.parse_args()

    cliente = Cliente(args.host, args.port)
    if not cliente.conectar():
        return

    if not cliente.login():
        return

    while True:
        opcion = menu()
        if opcion == "1":
            cliente.consulta_actual("ALL")
        elif opcion == "2":
            nodo = input("ID del nodo: ").strip()
            cliente.consulta_actual(nodo)
        elif opcion == "3":
            nodo = input("ID del nodo: ").strip()
            cliente.consulta_hist(nodo)
        elif opcion == "4":
            print("Hasta luego.")
            break
        else:
            print("Opcion invalida.")


if __name__ == "__main__":
    main()
