# Instrucciones de instalación y prueba — Fase 3

## 1. Compilar y arrancar el servidor

```bash
cd server
make
./server 9000 servidor.log
```

El servidor queda escuchando en el puerto `9000` mediante TCP y UDP y registra la actividad en `servidor.log`.

## 2. Instalar dependencia de Python

Para obtener la RAM real del sistema:

```bash
pip install psutil --break-system-packages
```

Si `psutil` no está instalado, el nodo utiliza un valor simulado de RAM como respaldo.

## 3. Levantar dos nodos

En terminales separadas:

```bash
cd nodes
python3 node.py --host localhost --port 9000 --id nodo01
```

```bash
cd nodes
python3 node.py --host localhost --port 9000 --id nodo02
```

Cada nodo se registra por TCP y envía un mensaje `ESTADO` por UDP cada cinco segundos. Si RAM supera 90% o la temperatura simulada supera 70 °C, se genera un `EVENTO` por TCP.

## 4. Levantar el cliente

```bash
cd clients
python3 client.py --host localhost --port 9000
```

Credenciales de prueba:

- `admin` / `admin123` → `ADMIN`
- `consulta` / `consulta123` → `CONSULTA`

Después del login se puede consultar el estado de todos los nodos, el estado de un nodo específico y el histórico de un nodo.

## 5. Pruebas para la sustentación

Se recomienda ejecutar:

- servidor + dos nodos + uno o dos clientes;
- consulta de todos los nodos;
- consulta de un nodo;
- consulta histórica;
- detener un nodo y esperar el timeout de 15 segundos;
- volver a levantar el nodo y verificar su recuperación;
- intentar un login incorrecto;
- enviar una consulta sin autenticación usando una herramienta externa si se desea probar el servidor;
- enviar un mensaje TCP mal formado y verificar que el servidor no se cierre.

La documentación detallada y las evidencias se encuentran en `docs/DOCUMENTACION.md`.
