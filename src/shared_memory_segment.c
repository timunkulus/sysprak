#include "shared_memory_segment.h"

#define _XOPEN_SOURCE 
#include<sys/types.h>
#include<sys/shm.h>
#include<sys/ipc.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include "performConnection.h"

//Anlegen von SHM
int shm_id (int size) {
    
    int shm_id;
    shm_id = shmget(IPC_PRIVATE, size, IPC_CREAT| 0777);
    if(shm_id < 0) {
        perror("SHM anlegen fehlgeschlagen! \n");
        return -1;
    }
    return shm_id;
}

//Anbinden von SHM
void *address_shm (int shm_id) {
    
    void *address_shm;
    address_shm = shmat (shm_id, (void*)0, 0);
    if(address_shm == (void*) -1) {
        perror("SHM anbinden fehlgeschlagen! \n");
        return "-1";
    }
    return address_shm;
}

//Loslösen von SHM
int dettach_shm (void *address){
    if(shmdt (address) < 0) {
        perror("SHM loslösen fehlgeschlagen! \n");
        return -1;}
    return 0;
}

//Loeschen von SHM
int delete_shm (int shm_id){
    if(shmctl (shm_id, IPC_RMID, 0) < 0) {
        perror("SHM loeschen fehlgeschlagen! \n");
        return -1;}
    return 0;
    
}

