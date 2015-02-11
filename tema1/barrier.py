"""
    Modulul bariera, implementat de echipa ASC
    si luat de la lab
"""
import sys
import threading
from threading import *

"""
    Ugly comments are ugly - nu folositi comentarii lungi pe aceeasi linie cu
    codul, aici sunt doar pentru 'teaching purpose'
"""

class SimpleBarrier():
    """ Bariera ne-reentranta, implementata folosind un semafor """

    def __init__(self, num_threads):
        self.num_threads = num_threads
        self.count_threads = self.num_threads
        self.counter_lock = Lock()      # protejam decrementarea numarului de threaduri
        self.threads_sem = Semaphore(0) # contorizam numarul de threaduri

    def wait(self):
        """
            Apelata de threaduri pentru a astepta sa ajunga toate in acest punct.
            Cand au ajuns toate, se vor debloca si continua executia.
        """
        with self.counter_lock:
            self.num_threads -= 1
            if self.num_threads == 0:   # a ajuns la bariera si ultimul thread
                for i in range(self.count_threads):
                    self.threads_sem.release()   # contorul semaforului devine count_threads
        self.threads_sem.acquire()     # n-1 threaduri se blocheaza aici
                                       # contorul semaforului se decrementeaza de count_threads ori

class ReusableBarrierCond():
    """ Bariera reentranta, implementata folosind o variabila conditie """

    def __init__(self, num_threads):
        self.num_threads = num_threads
        self.count_threads = self.num_threads
        self.cond = Condition(Lock())

    def wait(self):
        self.cond.acquire()       # intra in regiunea critica
        self.count_threads -= 1;
        if self.count_threads == 0:
            self.cond.notify_all() # trezeste toate thread-urile, acestea vor putea reintra  in regiunea critica dupa release
            self.count_threads=self.num_threads
        else:
            self.cond.wait();    # iese din regiunea critica, se blocheaza, cand se deblocheaza face acquire pe lock
        self.cond.release();     # iesim din regiunea critica

class ReusableBarrierSem():
    """ Bariera reentranta, implementata folosind semafoare """

    def __init__(self, num_threads):
        self.num_threads = num_threads
        self.count_threads1 = self.num_threads
        self.count_threads2 = self.num_threads

        self.counter_lock = Lock()       # protejam decrementarea numarului de threaduri
        self.threads_sem1 = Semaphore(0) # contorizam numarul de threaduri pentru prima etapa
        self.threads_sem2 = Semaphore(0) # contorizam numarul de threaduri pentru a doua etapa

    def wait(self):
        self.phase1()
        self.phase2()

    def phase1(self):
        with self.counter_lock:
            self.count_threads1 -= 1
            if self.count_threads1 == 0:
                for i in range(self.num_threads):
                    self.threads_sem1.release()
            self.count_threads2 = self.num_threads

        self.threads_sem1.acquire()

    def phase2(self):
        with self.counter_lock:
            self.count_threads2 -= 1
            if self.count_threads2 == 0:
                for i in range(self.num_threads):
                    self.threads_sem2.release()
            self.count_threads1 = self.num_threads

        self.threads_sem2.acquire()

