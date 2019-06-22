import unittest

import submit


class EvalSolutionTest(unittest.TestCase):

    def test_simple(self):
        self.assertEqual(48, submit.eval_solution(
            'WDWB(1,2)DSQDB(-3,1)DDDWWWWWWWSSEDSSDWWESSSSSAAAAAQQWWWWWWW'))

    def test_clone(self):
        self.assertEqual(28, submit.eval_solution(
            'WWWWWWWWWCDDDDDDESSSSSS#CDDDDDDDDESSSSSSSS#CSSDDDESSSSS#ESSSSSSSSSQDDDDD'))


if __name__ == '__main__':
    unittest.main()
