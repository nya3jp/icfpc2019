import sys

                #                        #           #               #            #                                                                   

_DIRS = [(1, 0), (0, 1), (-1, 0), (0, -1)]


def _create_polygon(grid, tSize):
    # TODO(yuizumi): Handle the case with grid[0][0].

    def cell(x, y):
        if (x >= 0) and (x < tSize) and (y >= 0) and (y < tSize):
            return grid[y][x]
        return '#'

    (x, y) = (0, 0)
    yield (x, y)

    (dx, dy) = (1, 0)
    while True:
        if dx > 0:
            if cell(x, y) == '#':
                yield (x, y)
                (dx, dy) = (-dy, dx)
            elif cell(x, y - 1) != '#':
                yield (x, y)
                (dx, dy) = (dy, -dx)
        elif dy > 0:
            if cell(x - 1, y) == '#':
                yield (x, y)
                (dx, dy) = (-dy, dx)
            elif cell(x, y) != '#':
                yield (x, y)
                (dx, dy) = (dy, -dx)
        elif dx < 0:
            if cell(x - 1, y - 1) == '#':
                yield (x, y)
                (dx, dy) = (-dy, dx)
            elif cell(x - 1, y) != '#':
                yield (x, y)
                (dx, dy) = (dy, -dx)
        elif dy < 0:
            if cell(x, y - 1) == '#':
                yield (x, y)
                (dx, dy) = (-dy, dx)
            elif cell(x - 1, y - 1) != '#':
                yield (x, y)
                (dx, dy) = (dy, -dx)

        (x, y) = (x + dx, y + dy)
        if (x, y) == (0, 0):
            return


def main():
    head, iSqs, oSqs = input().rstrip().split('#')
    bNum, eNum, tSize, vMin, vMax, mNum, fNum, dNum, rNum, cNum, xNum = (
        map(int, head.split(',')))
    iSqs = {tuple(map(int, s.split(','))) for s in iSqs[1:-1].split('),(')}
    oSqs = {tuple(map(int, s.split(','))) for s in oSqs[1:-1].split('),(')}

    grid = [([' '] * tSize) for _ in range(tSize)]

    for y in range(tSize):
        for x in range(tSize):
            if (x, y) in iSqs: grid[y][x] = '.'
            if (x, y) in oSqs: grid[y][x] = '*'

    for (x0, y0) in oSqs:
        min_dist = float('inf')
        best_dir = None
        for (dx, dy) in _DIRS:
            x, y = x0, y0
            okay = True
            while (x >= 0) and (x < tSize) and (y >= 0) and (y < tSize):
                if grid[y][x] == '#':
                    break
                if grid[y][x] == '.':
                    okay = False
                    break
                x, y = x + dx, y + dy
            if not okay:
                continue
            dist = abs(x - x0) + abs(y - y0)
            if dist < min_dist:
                min_dist = dist
                best_dir = (dx, dy)
        if not best_dir:
            sys.stderr.write('{}\n'.format((x0, y0)))
            continue
        for (dx, dy) in [best_dir]:
            x, y = x0, y0
            while (x >= 0) and (x < tSize) and (y >= 0) and (y < tSize):
                if grid[y][x] == '#':
                    break
                grid[y][x] = '#'
                x, y = x + dx, y + dy

    v = len(list(_create_polygon(grid, tSize)))

    def cell(x, y):
        if (x >= 0) and (x < tSize) and (y >= 0) and (y < tSize):
            return grid[y][x]
        return '#'

    for y in range(tSize):
        for x in range(tSize):
            if v >= vMin:
                break
            if grid[y][x] != ' ':
                continue
            count = 0
            for dy in [-1, 0, 1]:
                for dx in [-1, 0, 1]:
                    if cell(x + dx, y + dy) == '#':
                        count += 1
            if count == 3:
                grid[y][x] = '#'
                v += 4

    polygon = ','.join(f'({x},{y})' for x, y in _create_polygon(grid, tSize))
    wrappy = None
    boosters = []

    for y in range(tSize):
        for x in range(tSize):
            if grid[y][x] != ' ':
                continue
            if not wrappy:
                wrappy = f'({x},{y})'
                continue
            if mNum > 0:
                boosters.append(f'B({x},{y})')
                mNum -= 1
                continue
            if fNum > 0:
                boosters.append(f'F({x},{y})')
                fNum -= 1
                continue
            if dNum > 0:
                boosters.append(f'L({x},{y})')
                dNum -= 1
                continue
            if rNum > 0:
                boosters.append(f'R({x},{y})')
                rNum -= 1
                continue
            if cNum > 0:
                boosters.append(f'C({x},{y})')
                cNum -= 1
                continue
            if xNum > 0:
                boosters.append(f'X({x},{y})')
                xNum -= 1
                continue
            break

    boosters = ';'.join(boosters)
    sys.stdout.write(f'{polygon}#{wrappy}##{boosters}')


if __name__ == '__main__':
    main()
