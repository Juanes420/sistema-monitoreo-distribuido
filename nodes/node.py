#!/usr/bin/env python3
"""
Nodo SRMP (Server Resource Monitoring Protocol)

Se registra ante el servidor central por TCP, reporta telemetria periodica
por UDP (RAM real + temperatura simulada) y notifica eventos criticos por
TCP cuando se superan los umbrales definidos en la Fase 1 del diseno.

Uso:
    python3 node.py --host <host_servidor> --port <puerto> --id nodo01
"""

import argparse
import socket
import threading
import time
import random
import itertools
from datetime import datetime, timezone

try:
    import psutil
    HAS_PSUTIL = True
except ImportError:
    HAS_PSUTIL = False
    print("[AVISO] psutil no esta instalado; se simulara tambien el % de RAM.")
    print("        Instalar con: pip install psutil --break-system-packages")

# ---------------------------------------------------------------------------
# Umbrales de eventos criticos (definidos en docs/fase1.md)
# ---------------------------------------------------------------------------
UMBRAL_RAM = 90.0
UMBRAL_TEMP = 70.0
INTERVALO_ESTADO_SEG = 5
TEMP_MIN, TEMP_MAX = 40.0, 75.0

_msg_counter = itertools.count(1)


def timestamp():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def build_msg(tipo, id_origen, payload):
    msg_id = next(_msg_counter)
    ts = timestamp()
    return f"{tipo}|{id_origen}|{msg_id}|{ts}|{len(payload)}|{payload}"


def resolve_host(hostname):
    """Resuelve el nombre de dominio del servidor (requerimiento 4).
    Si falla, se informa el error y no se continua con una IP inventada."""
    try:
        infos = socket.getaddrinfo(hostname, None, socket.AF_INET)
        ip = infos[0][4][0]
        return ip
    except socket.gaierror as e:
        print(f"[ERROR] No se pudo resolver el nombre '{hostname}': {e}")
        return None


class Nodo:
    def __init__(self, host, port, node_id):
        self.host = host
        self.port = port
        self.id = node_id
        self.tcp_sock = None
        self.registered = False
        self.lock = threading.Lock()

    def conectar_y_registrar(self):
        ip = resolve_host(self.host)
        if ip is None:
            return False
        try:
            self.tcp_sock = socket.create_connection((ip, self.port), timeout=5)
        except (ConnectionRefusedError, socket.timeout, OSError) as e:
            print(f"[ERROR] No se pudo conectar al servidor {self.host}:{self.port} -> {e}")
            return False

        msg = build_msg("REG_NODO", self.id, "")
        self._enviar_tcp(msg)
        resp = self._leer_tcp()
        if resp and resp.startswith("ACK"):
            print(f"[OK] Nodo {self.id} registrado correctamente: {resp}")
            self.registered = True
            return True
        else:
            print(f"[ERROR] Registro rechazado por el servidor: {resp}")
            return False

    def _enviar_tcp(self, msg):
        try:
            self.tcp_sock.sendall((msg + "\n").encode())
        except OSError as e:
            print(f"[ERROR] Fallo al enviar por TCP: {e}")

    def _leer_tcp(self):
        try:
            data = self.tcp_sock.recv(4096).decode().strip()
            return data
        except (socket.timeout, OSError) as e:
            print(f"[ERROR] Fallo al leer respuesta TCP: {e}")
            return None

    def leer_ram(self):
        if HAS_PSUTIL:
            return psutil.virtual_memory().percent
        return random.uniform(20, 60)

    def leer_temperatura_simulada(self):
        return random.uniform(TEMP_MIN, TEMP_MAX)

    def enviar_estado_udp(self, ram, temp):
        payload = f"RAM={ram:.1f};TEMP={temp:.1f}"
        msg = build_msg("ESTADO", self.id, payload)
        udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            ip = resolve_host(self.host)
            if ip is None:
                return
            udp_sock.sendto(msg.encode(), (ip, self.port))
        except OSError as e:
            print(f"[ERROR] Fallo al enviar ESTADO por UDP: {e}")
        finally:
            udp_sock.close()

    def enviar_evento(self, tipo, valor):
        payload = f"TIPO={tipo};VAL={valor:.1f}"
        msg = build_msg("EVENTO", self.id, payload)
        with self.lock:
            self._enviar_tcp(msg)
            resp = self._leer_tcp()
        print(f"[EVENTO] {tipo} (valor={valor:.1f}) -> respuesta del servidor: {resp}")

    def loop_telemetria(self):
        while True:
            ram = self.leer_ram()
            temp = self.leer_temperatura_simulada()
            print(f"[ESTADO] RAM={ram:.1f}%  TEMP={temp:.1f}C")
            self.enviar_estado_udp(ram, temp)

            if ram > UMBRAL_RAM:
                self.enviar_evento("RAM_ALTA", ram)
            if temp > UMBRAL_TEMP:
                self.enviar_evento("TEMP_ALTA", temp)

            time.sleep(INTERVALO_ESTADO_SEG)


def main():
    parser = argparse.ArgumentParser(description="Nodo SRMP")
    parser.add_argument("--host", required=True, help="Nombre de dominio o host del servidor")
    parser.add_argument("--port", type=int, required=True, help="Puerto del servidor")
    parser.add_argument("--id", required=True, help="Identificador del nodo (ej. nodo01)")
    args = parser.parse_args()

    nodo = Nodo(args.host, args.port, args.id)

    while not nodo.conectar_y_registrar():
        print("[INFO] Reintentando registro en 3 segundos...")
        time.sleep(3)

    try:
        nodo.loop_telemetria()
    except KeyboardInterrupt:
        print(f"\n[INFO] Nodo {nodo.id} detenido por el usuario.")


if __name__ == "__main__":
    main()
