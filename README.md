# Sistema de Monitoreo y Control Distribuido (SRMP)

Proyecto de Internet: Arquitectura y Protocolos / Telemática 2026-2.


**Integrantes:**
-Juan Esteban Peña Rojas 
-Luis Miguel Mira Mejia 

## Estructura

- `server/`: servidor central en C con Berkeley Sockets.
- `nodes/`: nodos de monitoreo en Python.
- `clients/`: cliente de administración en Python.
- `docs/`: documentación y evidencias de pruebas.
- `INSTRUCCIONES.md`: guía rápida de instalación y ejecución.

## Ejecución rápida

### Servidor

```bash
cd server
make
./server 9000 servidor.log
```

### Nodo 1

```bash
cd nodes
python3 node.py --host localhost --port 9000 --id nodo01
```

### Nodo 2

```bash
cd nodes
python3 node.py --host localhost --port 9000 --id nodo02
```

### Cliente

```bash
cd clients
python3 client.py --host localhost --port 9000
```

Para obtener RAM real se requiere `psutil`:

```bash
pip install psutil --break-system-packages
```

La temperatura se simula porque la VM de VirtualBox utilizada durante las pruebas no expone los sensores térmicos físicos a Kali Linux.

## Documentación

La documentación completa se encuentra en [`docs/DOCUMENTACION.md`](docs/DOCUMENTACION.md).

## Credenciales de prueba

- `admin` / `admin123` → `ADMIN`
- `consulta` / `consulta123` → `CONSULTA`

Son credenciales de demostración del proyecto, no credenciales de producción.
