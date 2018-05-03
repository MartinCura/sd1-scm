#ifndef BROKER_SEMAPHORE_H
#define BROKER_SEMAPHORE_H


/* crear el set de semáforos (si no existe)
 */
int creasem(int identif);

/* adquirir derecho de acceso al set de semaforos existente
 */
int getsem(int identif);

/* inicializar el primer semáforo del set de semáforos
 */
int inisem(int semid, int val);

/* ocupar el primer semáforo del set   (p) WAIT
 */
int p(int semid);

/* liberar el primer semáforo del set  (v) SIGNAL
 */
int v(int semid);

/* eliminar el set de semáforos
 */
int delsem(int semid);


#endif //BROKER_SEMAPHORE_H
