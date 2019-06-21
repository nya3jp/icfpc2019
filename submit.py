#!/usr/bin/env python3

import argparse
import base64
import json
import os
import re
import sys
import urllib.request


ROOT_DIR = os.path.dirname(__file__)

API_KEY = 'c868a5215b6bfb6161c6a43363e62d45'


def _parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--problem', type=str, required=True, help='Problem name, e.g. prob-001')
    parser.add_argument('--solver', type=str, required=True, help='Solver name')
    parser.add_argument('--solution', type=str, required=True, help='Solution file path')
    return parser.parse_args()


class EvalError(Exception):
    pass


OP_RE = re.compile(r'[WSADZEQFLR]|[BT]\(-?\d+,-?\d+\)')


def _eval_solution(solution: str) -> int:
    score = 0
    pos = 0
    for m in OP_RE.finditer(solution):
        if m.start() != pos:
            raise EvalError('Solution contains unknown instruction %s at position %d' % (solution[pos], pos))
        score += 1
        pos = m.end()
    if pos != len(solution):
        raise EvalError('Solution contains unknown instruction %s at position %d' % (solution[pos], pos))
    return score


def main():
    options = _parse_args()

    with open(options.solution) as f:
        solution = f.read().strip()

    score = _eval_solution(solution)

    if score == 0:
        raise EvalError('Empty solution')

    data = {
        'solver': options.solver,
        'problem': options.problem,
        'solution': solution,
        'score': score,
    }
    headers = {
        'Content-Type': 'application/json',
        'Authorization': 'Basic %s' % base64.b64encode((':' + API_KEY).encode('ascii')).decode('ascii'),
    }
    req = urllib.request.Request(
        'http://34.85.124.237/submit',
        data=json.dumps(data).encode('utf-8'),
        method='POST',
        headers=headers)
    urllib.request.urlopen(req)
    

if __name__ == '__main__':
    sys.exit(main())
