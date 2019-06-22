#!/usr/bin/env python3

import argparse
import base64
import json
import os
import re
import sys
import urllib.error
import urllib.request


ROOT_DIR = os.path.dirname(__file__)

API_KEY = 'c868a5215b6bfb6161c6a43363e62d45'

OP_RE = re.compile(r'[WSADZEQFLRC]|[BT]\(-?\d+,-?\d+\)')
PROB_RE = re.compile(r'^prob-\d{3}$')


def _parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--problem', type=str, required=True, help='Problem name, e.g. prob-001')
    parser.add_argument('--solver', type=str, required=True, help='Solver name')
    parser.add_argument('--solution', type=str, required=True, help='Solution file path')
    return parser.parse_args()


class EvalError(Exception):
    pass


def _validate_replica(solution: str, offset: int):
    pc = 0
    for m in OP_RE.finditer(solution):
        if m.start() != pc:
            raise EvalError('Solution contains unknown instruction %s at position %d' % (solution[pc], offset + pc))
        pc = m.end()
    if pc != len(solution):
        raise EvalError('Solution contains unknown instruction %s at position %d' % (solution[pc], offset + pc))


def eval_solution(solution: str) -> int:
    replicas = solution.split('#')

    offset = 0
    for replica in replicas:
        _validate_replica(replica, offset)
        offset += len(replica) + 1

    steps = 0
    pcs = [0]
    while True:
        finish = True
        new_clones = 0
        for i, (replica, pc) in enumerate(zip(replicas, pcs)):
            if pc >= len(replica):
                continue
            finish = False
            if replica[pc] == 'C':
                new_clones += 1
            pc += 1
            if pc < len(replica) and replica[pc] == '(':
                pc += replica[pc:].find(')') + 1
            pcs[i] = pc
        if finish:
            break
        pcs.extend([0] * new_clones)
        steps += 1

    if len(pcs) != len(replicas):
        raise EvalError('Solution contains orphan replica')

    return steps


def main():
    options = _parse_args()

    if not PROB_RE.search(options.problem):
        raise Exception('Unknown problem name')

    with open(options.solution) as f:
        solution = f.read().strip()

    score = eval_solution(solution)

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

    try:
        with urllib.request.urlopen(req) as res:
            result = json.load(res)
    except urllib.error.HTTPError as e:
        body = e.fp.read().decode('utf-8')
        if 'Duplicate entry' in body:
            print('OK: Duplicated solution found')
            return
        sys.exit('SERVER ERROR: %s' % body)

    verified_score = result['score']
    last_best_score = result['lastBestScore']
    if last_best_score == 0:
        print('OK: Congrats! Submitted the first solution for %s with score %d' % (options.problem, verified_score))
    elif verified_score < last_best_score:
        print('OK: Congrats! Updated the best score of %s to %d (was: %d)' % (options.problem, verified_score, last_best_score))
    else:
        print('OK: Could not update the best score of %s: yours %d, best %d' % (options.problem, verified_score, last_best_score))


if __name__ == '__main__':
    sys.exit(main())
