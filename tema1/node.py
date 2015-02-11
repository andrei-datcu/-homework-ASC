"""
    Autor: Datcu Andrei Daniel, 331CC

    This module represents a cluster's computational node.

    Computer Systems Architecture Course
    Assignment 1 - Cluster Activity Simulation
    March 2014
"""

from node_threads import CommThread, MasterThread, Job
from threading import Semaphore, Event
from barrier import ReusableBarrierCond
from collections import deque


class Node:
    """
        Class that represents a cluster node with computation and storage
        functionalities.
    """

    def __init__(self, node_id, matrix_size):
        """
            Constructor.

            @type node_id: Integer
            @param node_id: an integer less than 'matrix_size' uniquely
                identifying the node
            @type matrix_size: Integer
            @param matrix_size: the size of the matrix A
        """
        self.node_id = node_id
        self.matrix_size = matrix_size
        self.datastore = None
        self.nodes = None
        self.commSem = Semaphore(0)
        self.jobList = deque()
        self.lastWriteJob = None
        self.commThreadList = []
        self.resultAvailable = Event()
        self.x = 0.0
        if (node_id == 0):
            self.lineOpBarrier = ReusableBarrierCond(matrix_size)
            self.pivotCheckBarrier = ReusableBarrierCond(matrix_size)
        else:
            self.lineOpBarrier = None
            self.pivotCheckBarrier = None

    def __str__(self):
        """
            Pretty prints this node.

            @rtype: String
            @return: a string containing this node's id
        """
        return "Node %d" % self.node_id

    def set_datastore(self, datastore):
        """
            Gives the node a reference to its datastore. Guaranteed to be called
            before the first call to 'get_x'.

            @type datastore: Datastore
            @param datastore: the datastore associated with this node
        """
        self.datastore = datastore
        max_req = datastore.get_max_pending_requests()
        if (max_req == 0):
            max_req = 4

        for i in xrange(max_req):
            self.commThreadList.append(CommThread(self.jobList, self.commSem,
                                                  self))
            self.commThreadList[-1].start()

    def set_nodes(self, nodes):
        """
            Informs the current node of the other nodes in the cluster.
            Guaranteed to be called before the first call to 'get_x'.

            @type nodes: List of Node
            @param nodes: a list containing all the nodes in the cluster
        """
        self.nodes = nodes
        if (self.node_id != 0):
            self.lineOpBarrier = nodes[0].lineOpBarrier
            self.pivotCheckBarrier = nodes[0].pivotCheckBarrier

    def get(self, op):
        """
            Intoarce valorea care se afla in datastore la coloana op
        """
        job = Job(op, Job.Read)
        self.jobList.append(job)
        self.commSem.release()
        job.doneEvent.wait()
        return job.result

    def set(self, op, value):
        """
            Pune in datastore, la coloana op, valorea value
        """
        job = Job(op, Job.Write, value)
        self.jobList.append(job)
        self.commSem.release()
        self.lastWriteJob = job

    def reorder_list(self):
        """
            Reordoneaza lista vecinilor dupa id-ul acestora.
            Aceasta metoda este apelata atunci cand doua noduri isi schimba
            pozitia pentru a evita impartirea la 0
        """
        self.nodes.sort(key=lambda node: node.node_id)

    def set_new_id(self, id):
        """
            Seteaza noul id, pentru a evita impartirea la 0
        """
        self.node_id = id

    def get_x(self):
        """
            Computes the x value corresponding to this node. This method is
            invoked by the tester. This method must block until the result is
            available.

            @rtype: (Float, Integer)
            @return: the x value and the index of this variable in the solution
                vector
        """
        mt = MasterThread(self, self.lineOpBarrier, self.pivotCheckBarrier)
        mt.start()
        mt.join()
        self.x = mt.result
        self.resultAvailable.set()
        return (mt.result, self.node_id)


    def shutdown(self):
        """
            Instructs the node to shutdown (terminate all threads). This method
            is invoked by the tester. This method must block until all the
            threads started by this node terminate.
        """
        for t in self.commThreadList:
            t.terminated = True

        for i in xrange(len(self.commThreadList)):
            self.commSem.release()

        for t in self.commThreadList:
            t.join()
