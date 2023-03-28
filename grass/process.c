/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: helper functions for managing processes
 */

#include "egos.h"
#include "process.h"
#include "syscall.h"
#include <string.h>

void intr_entry(int id);

void excp_entry(int id) {
    /* Student's code goes here (handle memory exception). */

    /* If id is for system call, handle the system call and return */
    //asm("csrr %0, mepc" : "=r"(mepc));
    if (id == 5 || id == 7){
        //memory fault
        INFO("process %d killed by memory exception", curr_pid);
        asm("csrw mepc, %0" ::"r"(0x800500C));
        return;
    }
    if (id == 8 || id == 11) {
        intr_entry(3);
        int mepc;
        asm("csrr %0, mepc" : "=r"(mepc));
        asm("csrw mepc, %0" ::"r"(mepc+4));
        //mret;
        return;
    }

    /* Otherwise, kill the process if curr_pid is a user application */

    /* Student's code ends here. */

    FATAL("excp_entry: kernel got exception %d", id);
}

void proc_init() {
    earth->intr_register(intr_entry);
    earth->excp_register(excp_entry);

    /* Student's code goes here (PMP memory protection). */

    /* Setup PMP TOR region 0x00000000 - 0x20000000 as r/w/x */
    asm("csrw pmp0cfg, %0" ::"r"(0x0F));
    // 0x0F
    asm("csrw pmpaddr0, %0" ::"r"((0x20000000) >> 2));

    /* Setup PMP NAPOT region 0x20400000 - 0x20800000 as r/-/x */
    asm("csrw pmp1cfg, %0" ::"r"(0x1D));
    
    asm("csrw pmpaddr1, %0" ::"r"((0x20400000 >> 2) | 0x7FFFF));

    /* Setup PMP NAPOT region 0x20800000 - 0x20C00000 as r/-/- */
    asm("csrw pmp2cfg, %0" ::"r"(0x11));
    asm("csrw pmpaddr2, %0" ::"r"((0x20800000 >> 2) | 0x7FFFF));

    /* Setup PMP NAPOT region 0x80000000 - 0x80004000 as r/w/- */
    asm("csrw pmp3cfg, %0" ::"r"(0x1B));
    asm("csrw pmpaddr3, %0" ::"r"((0x80000000 >> 2) | 0b0111111111111));//0xFFF));
    /* Student's code ends here. */

    /* The first process is currently running */
    proc_set_running(proc_alloc());
}

static void proc_set_status(int pid, int status) {
    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid == pid) proc_set[i].status = status;
}

int proc_alloc() {
    static int proc_nprocs = 0;
    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_UNUSED) {
            proc_set[i].pid = ++proc_nprocs;
            proc_set[i].status = PROC_LOADING;
            return proc_nprocs;
        }

    FATAL("proc_alloc: reach the limit of %d processes", MAX_NPROCESS);
}

void proc_free(int pid) {
    if (pid != -1) {
        earth->mmu_free(pid);
        proc_set_status(pid, PROC_UNUSED);
        return;
    }

    /* Free all user applications */
    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid >= GPID_USER_START &&
            proc_set[i].status != PROC_UNUSED) {
            earth->mmu_free(proc_set[i].pid);
            proc_set[i].status = PROC_UNUSED;
        }
}

void proc_set_ready(int pid) { proc_set_status(pid, PROC_READY); }
void proc_set_running(int pid) { proc_set_status(pid, PROC_RUNNING); }
void proc_set_runnable(int pid) { proc_set_status(pid, PROC_RUNNABLE); }
