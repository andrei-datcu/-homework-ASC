"""
    Autor: Datcu Andrei Daniel, 331CC
    Data: 22.03.2014
    Tema1, ASC

    Acest modul implementeaza cele doua tipuri de threaduri
    folosite de clasa Node:
        CommThread - Thread de comunicare cu alte noduri
        MasterThread - threadul principal, care se ocupa de calculul
        propriu-zis
"""

from threading import Thread, Event


class Job:

    """
        Clasa ce modeleaza un Job ce este transmis uni thread de comunicare.
        Jobul poate avea 2 tipuri(citire si scriere), pozitia coloanei,
        si eventual valoarea de scris. Semnalizarea terminarii jobului
        se va face prin intermediul eventului doneEvent de catre threadul
        ce a preluat jobul
    """

    #Job Types

    Read = 0
    Write = 1

    def __init__(self, pos, kind, value=None):
        self.pos = pos
        self.kind = kind
        self.value = value
        self.result = 0
        self.doneEvent = Event()


class CommThread(Thread):

    """
        Thread responsabil cu comunicarea. Threadul va fi controlat printr-un
        semafor gestionat de nodul parinte. Un Thread poate prelua un Job si
        semnalizeaza cand Job-ul este gata prin eventul atasat Job-ului
    """

    def __init__(self, job_list, wakeSem, owner_node):
        Thread.__init__(self)
        self.job_list = job_list
        self.wakeSem = wakeSem
        self.owner_node = owner_node
        self.terminated = False
        owner_node.datastore.register_thread(owner_node, self)

    def run(self):
        while (True):

            #astept sa se intample ceva: apare un job sau s-a incheiat treaba
            self.wakeSem.acquire()
            if (self.terminated):
                break

            job = self.job_list.popleft()

            if (job.kind == Job.Read):
                if (job.pos >= 0):
                    job.result = (self.owner_node.datastore
                                  .get_A(self.owner_node, job.pos))
                else:
                    job.result = (self.owner_node.datastore
                                  .get_b(self.owner_node))
            else:
                #este operatie de scriere
                if (job.pos >= 0):
                    self.owner_node.datastore.put_A(self.owner_node,
                                                    job.pos, job.value)
                else:
                    self.owner_node.datastore.put_b(self.owner_node,
                                                    job.value)

            #semnalam faptul ca rezultatul este disponibil
            job.doneEvent.set()


class MasterThread(Thread):

    """
        Thread master care se ocupa cu calculul propriu-zis
    """

    def __init__(self, owner_node, lineOpBarrier, pivotCheckBarrier):
        Thread.__init__(self)
        self.owner_node = owner_node
        owner_node.datastore.register_thread(owner_node, self)
        self.result = 0.0
        self.lineOpBarrier = lineOpBarrier
        self.pivotCheckBarrier = pivotCheckBarrier

    def alterLine(self, source_node, left_limit, factor):

        """
            Metoda care modifica linia nodului parinte, incepand
            cu coloana left_limit si pana la b. Se va scadea
            linia nodului source_node inmultita cu factor
        """

        for i in xrange(left_limit, self.owner_node.matrix_size):
            oldVal = self.owner_node.get(i)
            self.owner_node.set(i, oldVal - factor * source_node.get(i))

        oldB = self.owner_node.get(-1)
        self.owner_node.set(-1, oldB - source_node.get(-1) * factor)

    def run(self):
        other_nodes = self.owner_node.nodes
        owner_node = self.owner_node

        #Pasul1: facem 0 sub diagonala principala
        for i in xrange(self.owner_node.matrix_size):
            #Daca e randul nodului meu parinte sa-si faca linia 0 verific
            #ca pivotul sa fie diferit de 0

            if (i == owner_node.node_id):
                if (owner_node.get(i) == 0):
                    #Daca pivotul e 0 caut prima linie de sub mine care are
                    #pe coloana pivotului meu ceva diferit de 0

                    for j in xrange(i + 1, owner_node.matrix_size):
                        if (other_nodes[j].get(i) != 0):

                            #Schimbam doar id-urile

                            oldid = other_nodes[j].node_id
                            other_nodes[j].set_new_id(owner_node.node_id)
                            owner_node.set_new_id(oldid)

                            #reordonam lista nodurilor a tututor veciniilor
                            #dupa noile iduri
                            #creez o copie a listei vecinilor NESORTATA
                            temp_other_nodes = list(other_nodes)
                            for n in temp_other_nodes:
                                n.reorder_list()
                            break

            self.pivotCheckBarrier.wait()

            if (i < self.owner_node.node_id):
                #Daca mai sunt 0-uri de facut la stanga pivotului...

                ow_val = owner_node.get(i)
                if (ow_val != 0):
                    factor = ow_val / other_nodes[i].get(i)
                    owner_node.set(i, 0)
                    self.alterLine(other_nodes[i], i + 1, factor)

            #Daca mai avem scrieri nerealizate atunci le asteptam
            if (len(owner_node.jobList) != 0):
                if (owner_node.lastWriteJob is not None):
                    owner_node.lastWriteJob.doneEvent.wait()
            self.lineOpBarrier.wait()

        #Pasul2: astept rezultatele nodurilor de dupa mine pentru
        #        a-mi calcula propriul rezultat

        ssum = owner_node.get(-1)
        for i in xrange(owner_node.node_id + 1, owner_node.matrix_size):
            other_nodes[i].resultAvailable.wait()
            ssum -= owner_node.get(i) * other_nodes[i].x

        self.result = ssum / owner_node.get(owner_node.node_id)
