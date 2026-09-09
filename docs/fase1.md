# Fase 1 — Diseño y Arquitectura

## Sistema de Monitoreo y Control Distribuido — SRMP

### Server Resource Monitoring Protocol

**Curso:** Internet: Arquitectura y Protocolos, Telemática — 2026-2
**Integrantes:** Juan Esteban Peña, Luis Miguel Mira
**Fecha de entrega:** 9 de septiembre de 2026

---

## 1. Descripción general del problema

Una infraestructura de cómputo puede estar formada por varios servidores físicos o virtuales que necesitan ser supervisados de manera remota. El objetivo de este proyecto es desarrollar un sistema que permita centralizar esta información y consultar el estado de los diferentes servidores desde un solo lugar.

Para esto, los **nodos** representan los servidores que están siendo monitoreados. Cada nodo enviará periódicamente información sobre algunos de sus recursos a un **servidor central**. Además, los nodos podrán informar cuando ocurra algún evento importante, como superar un límite de temperatura o de uso de memoria.

Por otro lado, los **clientes de administración** podrán conectarse al servidor para consultar el estado actual de los nodos y revisar parte de su información histórica.

En esta primera propuesta, cada nodo reportará:

* **Porcentaje de uso de memoria RAM**, obtenido directamente del sistema operativo.
* **Temperatura de CPU**, que será simulada dentro de un rango de 40 °C a 75 °C. Se utilizará una temperatura simulada porque la forma de obtener este dato cambia dependiendo del sistema operativo y del entorno donde se ejecute el programa.

Los nodos generarán un evento cuando ocurra alguna de las siguientes situaciones:

* El uso de RAM supera el 90 %.
* La temperatura supera los 70 °C.
* El nodo se desconecta o vuelve a conectarse al servidor.

El servidor central será el encargado de mantener la información más reciente de cada nodo, guardar un historial de las mediciones y responder las consultas realizadas por los clientes.

---

## 2. Arquitectura propuesta

El sistema estará compuesto por dos nodos, un servidor central y uno o varios clientes de administración.



### Reglas de la arquitectura

* Los nodos solo se comunican directamente con el servidor.
* Los clientes solo se comunican con el servidor.
* No existe comunicación directa entre un nodo y un cliente.
* El servidor es el encargado de mantener el estado general de los nodos.
* Los nodos utilizarán TCP para registrarse y enviar eventos críticos.
* Los nodos utilizarán UDP para enviar las mediciones periódicas.
* Los clientes utilizarán TCP para autenticarse y realizar consultas.
* La autenticación se manejará mediante un módulo separado de la lógica principal del servidor.

---

## 3. Entidades participantes

| Entidad                       | Lenguaje      | Responsabilidad                                                                                                                            |
| ----------------------------- | ------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| **Nodo**                      | Python        | Registrarse ante el servidor, obtener las métricas de RAM y temperatura, enviar información periódicamente y reportar eventos importantes. |
| **Servidor central**          | C             | Recibir los mensajes de los nodos, mantener el estado e historial, responder consultas y manejar varias conexiones al mismo tiempo.        |
| **Cliente de administración** | Python        | Iniciar sesión, consultar el estado actual de los nodos y consultar información histórica.                                                 |
| **Módulo de autenticación**   | Independiente | Validar las credenciales de los clientes y determinar el perfil de acceso.                                                                 |

Para los clientes se proponen inicialmente dos perfiles:

* **Administrador:** puede consultar la información actual e histórica de todos los nodos.
* **Consulta:** puede consultar la información disponible, pero tendrá permisos más limitados.

La forma exacta en que se manejarán estos perfiles se definirá durante la implementación.

---

## 4. Tipos de mensajes

El protocolo **SRMP** tendrá diferentes tipos de mensajes dependiendo de la entidad que los envía.

### Mensajes enviados por los nodos

| Mensaje    | Código | Descripción                                                 |
| ---------- | ------ | ----------------------------------------------------------- |
| `REG_NODO` | `0x01` | Permite registrar un nodo ante el servidor.                 |
| `ESTADO`   | `0x02` | Envía periódicamente las métricas del nodo.                 |
| `EVENTO`   | `0x03` | Informa sobre un evento importante o una situación crítica. |

### Mensajes enviados por los clientes

| Mensaje           | Código | Descripción                                      |
| ----------------- | ------ | ------------------------------------------------ |
| `LOGIN`           | `0x10` | Solicita la autenticación del cliente.           |
| `CONSULTA_ACTUAL` | `0x11` | Solicita el estado actual de uno o varios nodos. |
| `CONSULTA_HIST`   | `0x12` | Solicita información histórica de un nodo.       |

### Mensajes enviados por el servidor

| Mensaje       | Código | Descripción                                                                   |
| ------------- | ------ | ----------------------------------------------------------------------------- |
| `ACK`         | `0x80` | Confirma que un mensaje que requiere confirmación fue recibido correctamente. |
| `RESP_ESTADO` | `0x81` | Contiene la respuesta a una consulta sobre el estado actual.                  |
| `RESP_HIST`   | `0x82` | Contiene información histórica de un nodo.                                    |
| `ERROR`       | `0xFF` | Informa que ocurrió algún problema al procesar el mensaje.                    |

El mensaje `ACK` será obligatorio para los registros y eventos críticos. Las mediciones periódicas `ESTADO` no necesitan una confirmación obligatoria porque pueden tolerar la pérdida ocasional de una medición.

---

## 5. Sintaxis preliminar de los mensajes

Inicialmente se propone utilizar mensajes de texto separados por el carácter `|`. Esto facilita la lectura y las pruebas durante el desarrollo.

La estructura general será:

```text
TIPO|ID_ORIGEN|ID_MENSAJE|TIMESTAMP|LONGITUD_PAYLOAD|PAYLOAD
```

Los campos tendrán la siguiente función:

* `TIPO`: indica qué operación se está realizando.
* `ID_ORIGEN`: identifica al nodo o cliente que envía el mensaje.
* `ID_MENSAJE`: identificador único del mensaje.
* `TIMESTAMP`: fecha y hora en que se generó el mensaje.
* `LONGITUD_PAYLOAD`: indica el tamaño del contenido adicional.
* `PAYLOAD`: contiene los datos específicos de cada mensaje.

### Ejemplos

**Registro de un nodo:**

```text
REG_NODO|nodo01|1001|2026-09-09T10:00:00|0|
```

**Envío de información periódica:**

```text
ESTADO|nodo01|1002|2026-09-09T10:00:05|18|RAM=62;TEMP=58.3
```

**Evento crítico:**

```text
EVENTO|nodo01|1003|2026-09-09T10:00:07|24|TIPO=TEMP_ALTA;VAL=71.2
```

**Confirmación del servidor:**

```text
ACK|nodo01|1003|2026-09-09T10:00:07|2|OK
```

**Consulta de histórico:**

```text
CONSULTA_HIST|cliente01|2001|2026-09-09T10:05:00|10|nodo=nodo01
```

**Mensaje de error:**

```text
ERROR|nodo99|1004|2026-09-09T10:00:05|20|NODO_NO_REGISTRADO
```

Esta sintaxis es preliminar. Durante la Fase 2 se definirán con mayor precisión las reglas de validación, los tamaños de los campos y el manejo de caracteres especiales.

---

## 6. Reglas básicas de comunicación

1. Un nodo debe registrarse mediante `REG_NODO` antes de enviar mensajes `ESTADO` o `EVENTO`.

2. Si el servidor recibe información de un nodo que no está registrado, responderá con un mensaje `ERROR` indicando `NODO_NO_REGISTRADO`.

3. Todo mensaje `EVENTO` debe ser confirmado por el servidor mediante un `ACK`.

4. Los mensajes `ESTADO` no necesitan confirmación obligatoria, ya que corresponden a información que se actualiza periódicamente.

5. Un cliente debe enviar primero un mensaje `LOGIN`. Solo después de una autenticación correcta podrá realizar consultas.

6. Si las credenciales del cliente son incorrectas, el servidor enviará un mensaje `ERROR`.

7. Los mensajes con un formato incorrecto serán rechazados mediante un mensaje `ERROR`. Este error no debe provocar que el servidor termine su ejecución ni afectar las conexiones de los demás usuarios.

8. Cada mensaje tendrá un `ID_MENSAJE`. Esto permitirá identificar mensajes repetidos y facilitará el control de los mensajes enviados mediante UDP.

9. El servidor deberá resolver los nombres de dominio utilizados por el sistema y no tendrá direcciones IP escritas directamente en el código.

10. Si ocurre un error durante la resolución de un nombre de dominio, el servidor deberá manejar la situación y continuar funcionando.

---

## 7. Máquinas de estado

### 7.1 Estado de un nodo

Desde el punto de vista del servidor, un nodo podrá encontrarse en los siguientes estados:



Un nodo comienza como desconocido. Después de registrarse correctamente pasa a `Registrado`. Cuando el servidor recibe su primera medición, pasa a `Activo`.

Si durante un tiempo determinado no se reciben mediciones del nodo, el servidor lo marcará como `Inactivo`. Si posteriormente vuelve a recibir información, el nodo vuelve a estar `Activo`.

---

### 7.2 Sesión de un cliente



El cliente primero establece la conexión con el servidor y debe autenticarse correctamente. Una vez autenticado puede realizar las consultas permitidas según su perfil.

---

## 8. Análisis preliminar sobre TCP y UDP

La elección del protocolo de transporte dependerá del tipo de información que se esté enviando.

| Tipo de mensaje | Frecuencia                          | Criticidad | Tolerancia a pérdida     | Protocolo |
| --------------- | ----------------------------------- | ---------- | ------------------------ | --------- |
| `REG_NODO`      | Una vez al registrarse              | Alta       | No tolera pérdida        | **TCP**   |
| `ESTADO`        | Cada pocos segundos                 | Baja       | Tolera pérdida ocasional | **UDP**   |
| `EVENTO`        | Cuando ocurre una situación crítica | Alta       | No tolera pérdida        | **TCP**   |
| `LOGIN`         | Al iniciar sesión                   | Alta       | No tolera pérdida        | **TCP**   |
| `CONSULTA_*`    | Bajo demanda                        | Alta       | No tolera pérdida        | **TCP**   |

### Justificación

Se propone utilizar una combinación de TCP y UDP.

Las mediciones periódicas (`ESTADO`) utilizarán **UDP**, porque se envían con frecuencia, son pequeñas y perder una medición no representa un problema grave. En pocos segundos el nodo enviará una nueva medición con información más actualizada.

En cambio, el registro de un nodo (`REG_NODO`) y los eventos críticos (`EVENTO`) utilizarán **TCP**, porque estos mensajes son importantes para el funcionamiento del sistema y no sería conveniente perderlos.

Las comunicaciones de los clientes también utilizarán **TCP**, ya que las solicitudes de autenticación y las consultas necesitan recibir una respuesta correcta y en el orden en que fueron realizadas.

De esta manera, no se utiliza un único protocolo de transporte para todo el sistema, sino que se escoge el que mejor se adapta a cada tipo de información.

---

## 9. Autenticación

El sistema contará con un mecanismo de autenticación para controlar el acceso de los clientes.

Antes de realizar una consulta, el cliente deberá enviar un mensaje `LOGIN` con sus credenciales. El servidor verificará estas credenciales mediante un módulo de autenticación separado de la lógica principal del servidor.

Inicialmente se manejarán dos perfiles:

* **Administrador:** tendrá acceso a las consultas actuales e históricas.
* **Consulta:** tendrá acceso únicamente a las consultas permitidas para este perfil.

La implementación concreta del mecanismo de autenticación se definirá durante la Fase 2, teniendo en cuenta que el enunciado recomienda evitar que todo el sistema de usuarios dependa únicamente de un archivo local o de una base de datos propia dentro de la aplicación principal.

---

## 10. Conclusión de la propuesta

En esta primera fase se definió la arquitectura general del sistema, las entidades que participarán, los principales mensajes del protocolo SRMP, una sintaxis preliminar, las reglas básicas de comunicación, las máquinas de estado y la elección inicial de TCP y UDP.

La siguiente fase estará enfocada en llevar este diseño a una implementación funcional utilizando sockets. El servidor será desarrollado en C mediante la API de Sockets Berkeley y los nodos y clientes podrán desarrollarse en Python.
