# Sistema de colas de mensajes

## Sistemas Distribuidos I -- FIUBA


### Compile

```bash
$ mkdir bin/
$ cd bin/
$ cmake ..
$ make
```

### Run

Se puede correr un único servidor o varios en forma de anillo.
Estos pueden estar en cualquier lado pero donde se corra el client-side debe antes lanzarse un broker local, que medie con el server.

#### Modo independiente
Para tener solo un server, en distintas terminales correr

```bash
(1)$ ./scm-server

(2)$ ./local-broker

(3)$ ./scm-client [comando] [[argumentos]]
```

#### Modo ring
Se debe lanzar el primer server únicamente con su id, y cada subsiguiente indicándole a cuál de los anteriores se conectará.
En cada sistema donde se quiera iniciar un server y un broker local, correr en distintas terminales:
```bash
(1)$ scm-server <server id> [<next server id>] [<next server IP si no es local>]

(2)$ local-broker <server id> [-p <server IP si no es local>]

(3)$ cd s<server id>/
(3)$ ./scm-client [comando] [[argumentos]]
```
En cada host se envían los comandos a ```scm-client``` dentro de la carpeta hecha para el servidor local (por si se quieren lanzar varios servers en el mismo sistema).

