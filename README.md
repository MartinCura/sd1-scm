# sd1-scm
Sistema de colas de mensajes, para Sistemas Distribuidos 1 -- FIUBA

```bash
$ mkdir bin/
$ cd bin/
$ cmake ..
$ make
```

Para correr solo un server, en distintas terminales correr scm-server, después local-broker, y en otra se puede usar el sistema enviando comandos a scm-client.

Para correr en ring:
Se debe correr el primer server únicamente con su id, y cada subsiguiente indicándole a cuál se conectará.
```
$ scm-server <server id> [<next server id>]

$ local-broker <server id>

$ cd s<server id>/
```
En cada host se envían los comandos a scm-client dentro de la carpeta hecha para el servidor local (por si se quieren lanzar varios servers en el mismo sistema).
