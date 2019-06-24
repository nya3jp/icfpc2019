import sys
import subprocess
import numpy
import io

class WrapConnector():
    def __init__(self,
                 prog_name
        ):
        self.phandle = subprocess.Popen(prog_name, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.layers = 11
        self.fieldview = 13
        self.current_state = None

    def reset(self):
        print("issue reset")
        self.phandle.stdin.write(b'@')
        self.phandle.stdin.flush()
        rstream = self.phandle.stdout.read(1)
        assert rstream == b'@', repr(rstream)
        self._read_status()
        print("reset completed")

    def get_status(self):
        assert self.current_state is not None
        return self.current_state
        
    def step(self, action):
        #print("step (action = ", action, ")")
        self.phandle.stdin.write(bytearray((action,)))
        self.phandle.stdin.flush()
        (reward, terminate) = self.read_reward_terminate()
        #print("reward = ", reward)
        state = self._read_status()
        #print("return step")
        return (state, reward, terminate)
        
    def _read_status(self):
        #print("read_status")
        buf = self.phandle.stdout.read(1 * self.layers * self.fieldview * self.fieldview)
        bmp = numpy.frombuffer(buf, dtype='u1',
                               count=self.layers * self.fieldview * self.fieldview)
        bmp = bmp.reshape((self.layers, self.fieldview, self.fieldview))
        bmp = numpy.transpose(bmp)
        ncollect = 4 # FLRC
        buf = self.phandle.stdout.read(1 * ncollect)
        #print(repr(buf))
        collects = numpy.frombuffer(buf, dtype='u1',
                                    count=ncollect)
        buf = self.phandle.stdout.read(1)
        assert buf == b'.', "buf=" + repr(buf)
        #print("done read_status")
        bmp = bmp.reshape((-1,))
        #print("bmp:", bmp.shape)
        #print("col:", collects.shape)
        ret = numpy.concatenate((bmp, collects))
        #print("ret:", ret.shape)
        self.current_state = ret
        return ret

    def read_reward_terminate(self):
        #print("read_reward_terminate")
        buf = self.phandle.stdout.read(4 * 1)
        #print(repr(buf))
        reward = numpy.frombuffer(buf, dtype="i4", count=1)
        buf = self.phandle.stdout.read(4 * 1)
        #print(repr(buf))
        terminate = numpy.frombuffer(buf, dtype="i4", count=1)
        #print("done read_reward_terminate")
        return (int(reward[0]), int(terminate[0]))
        
    def close(self):
        self.phandle.stdin.write('Q')
        self.phandle.stdin.flush()
        self.phandle.communicate()
        
