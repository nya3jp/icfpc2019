"""Wrapped atari environment, multi-process to speed up"""

import multiprocessing as mp
from multiprocessing import Pool, sharedctypes
import numpy as np
import sys
#from preprocessor import Preprocessor
import time
import wrappipe

class Atari_ParallelWorker(mp.Process):
    def __init__(self,
                i,
                env_name,
                pipe,
                window_size,
                input_shape,
                num_frame_per_action,
                max_episode_length,
                state,
                lock):
        super(Atari_ParallelWorker, self).__init__()
        self.id = i
        self.pipe = pipe
        self.window_size = window_size
        self.num_frame_per_action = num_frame_per_action
        self.max_episode_length = max_episode_length
        self.state = state
        self.lock = lock
        self.env = wrappipe.WrapConnector(env_name)
        #self.preprocessor = Preprocessor(window_size, input_shape)
        self.time = 0
        self._reset()

    def _take_action(self, action):
        #print("take_action")
        reward = 0
        old_state = self.env.get_status()
        new_state, intermediate_reward, is_terminal = self.env.step(action)
        if is_terminal:
            self._reset()
        
        if self.time > self.max_episode_length:
            is_terminal = True
            self._reset()
        # write 'sara' into mp.Array
        np.ctypeslib.as_array(self.state['old_state'])[self.id] = old_state
        np.ctypeslib.as_array(self.state['action'])[self.id] = action
        np.ctypeslib.as_array(self.state['reward'])[self.id] = reward
        np.ctypeslib.as_array(self.state['new_state'])[self.id] = new_state
        np.ctypeslib.as_array(self.state['is_terminal'])[self.id] = is_terminal

    def _reset(self):
        print("***batchEnv/_reset")
        self.env.reset()
        self.time = 0
        # DO some burn in step to skip the start of the game
        #state, _, _, _ = self.env.step(0)
            #self.preprocessor.process_state_for_memory(state)
            
    def run(self):
        print("***batchEnv/run")
        print('Environment worker %d: run'%(self.id,))
        # This lock to ensure all the process prepared before take actions
        self.lock.release()
        while True:
            command, context = self.pipe.recv()
            if command == 'CLOSE':
                self.env.close()
                self.pipe.close()
                break
            elif command == 'ACTION':
                self._take_action(action=context)
                self.lock.release()
            elif command == 'RESET':
                self._reset()
                self.lock.release()
            else:
                raise NotImplementedError()

class BatchEnvironment:
    def __init__(self,
                env_name,
                num_process,
                window_size,
                input_shape,
                num_frame_per_action,
                max_episode_length):
        self.num_process = num_process
        self.env_name = env_name
        self.workers = []
        self.pipes = []
        self.locks = []

        def get_multiprocess_numpy(dtype, shape):
            tmp = np.ctypeslib.as_ctypes(np.zeros(shape, dtype=dtype))
            return sharedctypes.Array(tmp._type_, tmp, lock=False) 


        windowsize = 13
        nlayers = 11
        ncollect = 4
        payload = windowsize*windowsize*nlayers+ncollect

        self.state = {
            'old_state': get_multiprocess_numpy(np.uint8, 
                                                shape=(num_process,payload)),
            'action': get_multiprocess_numpy(np.uint8,
                                             shape=(num_process,)),
            'reward': get_multiprocess_numpy(np.int16,
                                             shape=(num_process,)),
            'new_state': get_multiprocess_numpy(np.uint8,
                                                 shape=(num_process,payload)),
            'is_terminal': get_multiprocess_numpy(np.uint8,
                                                  shape=(num_process,)),
            }

        for i in range(num_process):
            parent_pipe, child_pipe = mp.Pipe()
            lock = mp.Lock()
            self.pipes.append(parent_pipe)
            self.locks.append(lock)
            # This lock to ensure all the process prepared before take actions
            lock.acquire()
            worker = Atari_ParallelWorker(i, env_name, child_pipe, window_size,
                    input_shape, num_frame_per_action, max_episode_length, self.state, lock)
            worker.start()
            self.workers.append(worker)
       
    def take_action(self, action_list):
        assert len(action_list) == self.num_process
        for pipe, action, lock in zip(self.pipes, action_list, self.locks):
            lock.acquire()
            pipe.send(('ACTION', action))
            

    def reset(self):
        for pipe, lock in zip(self.pipes, self.locks):
            lock.acquire()
            pipe.send(('RESET', None))
            

    def get_state(self):
        for lock in self.locks:
            lock.acquire()
        
        old_state = np.ctypeslib.as_array(self.state['old_state'])
        action = np.ctypeslib.as_array(self.state['action'])
        reward = np.ctypeslib.as_array(self.state['reward'])
        new_state = np.ctypeslib.as_array(self.state['new_state'])
        is_terminal = np.ctypeslib.as_array(self.state['is_terminal'])
        
        for lock in self.locks:
            lock.release()
        return (np.copy(old_state),
                action, reward,
                np.copy(new_state),
                is_terminal)

    def close(self):
        for worker in self.workers:
            worker.terminate()
