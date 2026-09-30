# Sistema de Monitoreo y Control Distribuido — SRMP
 
**Server Resource Monitoring Protocol**
 
Proyecto de la asignatura *Internet: Arquitectura y Protocolos* (Telemática) — 2026-2.
 
## Integrantes
 
- Juan Esteban Peña
- Luis Miguel Mira
## Descripción
 
Sistema distribuido donde varios **nodos** (servidores simulados) reportan periódicamente
su uso de RAM y temperatura de CPU a un **servidor central**, y notifican eventos críticos
cuando se superan ciertos umbrales. Uno o varios **clientes de administración** pueden
autenticarse y consultar el estado actual e histórico de los nodos.
 
- **Servidor:** C (sockets Berkeley), concurrente por hilos.
- **Nodos y clientes:** Python.
- **Transporte:** combinación de TCP (registro, eventos, autenticación, consultas) y UDP
  (telemetría periódica). Justificación completa en la especificación del protocolo.
## Estructura del repositorio
 
```
.
├── clients/     Codigo de los clientes (python)
├── server/      Código del servidor central (C)
├── nodes/       Código de los nodos (Python)
```

**Fase 1 — Diseño y arquitectura**: ver [`docs/fase1.md`](docs/fase1.md)
