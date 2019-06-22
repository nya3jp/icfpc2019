use chrono::prelude::*;
use num_rational::Rational;
use regex::Regex;
use std::cmp::{max, min};
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::env;
use std::fs::{self, File};
use std::io::prelude::*;
use std::mem::swap;
use std::path::{Path, PathBuf};
use structopt::StructOpt;

const ISLAND_SIZE_THRESHOLD: usize = 50;

type Result<T> = std::result::Result<T, Box<std::error::Error>>;

// Board format
// bit 0: Wall
// bit 1: Painted
// bit 4-7: Booster
type Board = Vec<Vec<u8>>;

#[derive(Debug, StructOpt)]
enum Opt {
    #[structopt(name = "solve")]
    Solve {
        #[structopt(short = "s")]
        show_solution: bool,

        input: PathBuf,
    },

    #[structopt(name = "pack")]
    Pack,
}

type Pos = (i64, i64);

// const VECT: &[Pos] = &[(-1, 0), (1, 0), (0, 1), (0, -1)];
const VECT: &[Pos] = &[(-1, 0), (0, 1), (1, 0), (0, -1)];

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
enum Booster {
    B,
    F,
    L,
    X,
    R,
    C,
}

#[derive(Debug)]
enum Command {
    Move(i64, i64),
    Nop,
    Turn(bool),
    AttachManip(i64, i64),
    AttachWheel,
    StartDrill,
}

#[derive(Debug, Clone)]
struct Input {
    map: Vec<Pos>,
    init_pos: Pos,
    obstacles: Vec<Vec<Pos>>,
    boosters: Vec<(Booster, Pos)>,
}

fn parse_pos(s: &str) -> Result<Pos> {
    // dbg!(s);
    let s = s
        .chars()
        .map(|c| if c.is_ascii_digit() { c } else { ' ' })
        .collect::<String>();
    let mut it = s.split_whitespace().map(|w| w.parse().unwrap());
    Ok((it.next().unwrap(), it.next().unwrap()))
}

fn parse_pos_list(s: &str) -> Result<Vec<Pos>> {
    // dbg!(s);
    s.split("),(")
        .filter(|w| w.trim() != "")
        .map(parse_pos)
        .collect()
}

fn parse_pos_list_list(s: &str) -> Result<Vec<Vec<Pos>>> {
    // dbg!(s);
    s.split(';')
        .filter(|w| w.trim() != "")
        .map(parse_pos_list)
        .collect()
}

fn parse_booster(s: &str) -> Result<(Booster, Pos)> {
    let ty = match &s[0..1] {
        "B" => Booster::B,
        "F" => Booster::F,
        "L" => Booster::L,
        "X" => Booster::X,
        "R" => Booster::R,
        "C" => Booster::C,
        _ => unreachable!(),
    };

    let pos = parse_pos(&s[1..])?;

    Ok((ty, pos))
}

fn parse_booster_list(s: &str) -> Result<Vec<(Booster, Pos)>> {
    s.split(';')
        .filter(|w| w.trim() != "")
        .map(parse_booster)
        .collect()
}

fn read_input(path: &Path) -> Result<Input> {
    let mut s = String::new();
    let _ = File::open(path)?.read_to_string(&mut s)?;
    let s = s.trim().to_string();

    let mut iter = s.split('#');

    let ret = Input {
        map: parse_pos_list(iter.next().unwrap())?,
        init_pos: parse_pos(iter.next().unwrap())?,
        obstacles: parse_pos_list_list(iter.next().unwrap())?,
        boosters: parse_booster_list(iter.next().unwrap())?,
    };

    Ok(ret)
}

fn normalize(input: &mut Input) -> (i64, i64) {
    let min_x = *input.map.iter().map(|(x, y)| x).min().unwrap();
    let max_x = *input.map.iter().map(|(x, y)| x).max().unwrap();
    let min_y = *input.map.iter().map(|(x, y)| y).min().unwrap();
    let max_y = *input.map.iter().map(|(x, y)| y).max().unwrap();

    for i in 0..input.map.len() {
        input.map[i].0 -= min_x;
        input.map[i].1 -= min_y;
    }

    input.init_pos.0 -= min_x;
    input.init_pos.1 -= min_y;

    for i in 0..input.obstacles.len() {
        for j in 0..input.obstacles[i].len() {
            input.obstacles[i][j].0 -= min_x;
            input.obstacles[i][j].1 -= min_y;
        }
    }

    for i in 0..input.boosters.len() {
        input.boosters[i].1 .0 -= min_x;
        input.boosters[i].1 .1 -= min_y;
    }

    (max_x - min_x, max_y - min_y)
}

fn print_bd(bd: &Vec<Vec<u8>>) {
    for y in (0..bd.len()).rev() {
        for x in 0..bd[y].len() {
            let c = if bd[y][x] >> 4 == 1 {
                'B'
            } else if bd[y][x] >> 4 == 2 {
                'F'
            } else if bd[y][x] >> 4 == 3 {
                'L'
            } else if bd[y][x] >> 4 == 4 {
                'X'
            } else if bd[y][x] >> 4 == 5 {
                'X'
            } else if bd[y][x] & 4 != 0 {
                '+'
            } else if bd[y][x] & 2 != 0 {
                '.'
            } else if bd[y][x] & 1 != 0 {
                '#'
            } else {
                ' '
            };
            eprint!("{}", c);
        }
        eprintln!();
    }
    eprintln!();
}

fn encode_commands(ans: &[Command]) -> String {
    let mut ret = String::new();
    for cmd in ans.iter() {
        match cmd {
            Command::Move(0, 1) => ret += "W",
            Command::Move(0, -1) => ret += "S",
            Command::Move(-1, 0) => ret += "A",
            Command::Move(1, 0) => ret += "D",
            Command::Move(_, _) => panic!("Invalid move: {:?}", cmd),

            Command::Nop => ret += "Z",
            Command::Turn(true) => ret += "E",
            Command::Turn(false) => ret += "Q",
            Command::AttachManip(dx, dy) => ret += &format!("B({},{})", dx, dy),
            Command::AttachWheel => ret += "F",
            Command::StartDrill => ret += "L",
        }
    }
    ret
}

fn build_map(input: &Input, w: i64, h: i64) -> Vec<Vec<u8>> {
    let mut bd = vec![vec![1; w as usize]; h as usize];
    for y in 0..h {
        let mut ss = BTreeSet::new();
        for i in 0..input.map.len() {
            let (x1, y1) = input.map[i];
            let (x2, y2) = input.map[(i + 1) % input.map.len()];

            if x1 != x2 {
                continue;
            }

            let (y1, y2) = (min(y1, y2), max(y1, y2));

            if y1 <= y && y < y2 {
                ss.insert(x1);
            }
        }

        let ss = ss.into_iter().collect::<Vec<_>>();
        assert!(ss.len() % 2 == 0);

        // dbg!((y, &ss));

        for i in 0..ss.len() / 2 {
            for j in ss[i * 2]..ss[i * 2 + 1] {
                bd[y as usize][j as usize] = 0;
            }
        }
    }

    for y in 0..h {
        let mut ss = BTreeSet::new();
        for j in 0..input.obstacles.len() {
            for i in 0..input.obstacles[j].len() {
                let (x1, y1) = input.obstacles[j][i];
                let (x2, y2) = input.obstacles[j][(i + 1) % input.obstacles[j].len()];

                if x1 != x2 {
                    continue;
                }

                let (y1, y2) = (min(y1, y2), max(y1, y2));

                if y1 <= y && y < y2 {
                    ss.insert(x1);
                }
            }
        }

        let ss = ss.into_iter().collect::<Vec<_>>();
        assert!(ss.len() % 2 == 0);

        // dbg!((y, &ss));

        for i in 0..ss.len() / 2 {
            for j in ss[i * 2]..ss[i * 2 + 1] {
                bd[y as usize][j as usize] = 1;
            }
        }
    }

    for (typ, (x, y)) in input.boosters.iter() {
        match typ {
            Booster::B => bd[*y as usize][*x as usize] |= 1 << 4,
            Booster::F => bd[*y as usize][*x as usize] |= 2 << 4,
            Booster::L => bd[*y as usize][*x as usize] |= 3 << 4,
            Booster::X => bd[*y as usize][*x as usize] |= 4 << 4,
            Booster::R => bd[*y as usize][*x as usize] |= 5 << 4,
            Booster::C => bd[*y as usize][*x as usize] |= 6 << 4,
        }
    }

    bd
}

// solution

fn nearest(bd: &Vec<Vec<u8>>, x: i64, y: i64, has_prios: bool) -> Option<(i64, i64)> {
    let h = bd.len() as i64;
    let w = bd[0].len() as i64;

    /*
        let mut q = VecDeque::new();
        q.push_back((x, y));

        let mut found = None;
        let mut prev = BTreeMap::<Pos, Pos>::new();

        while let Some((cx, cy)) = q.pop_front() {
            for &(dx, dy) in VECT.iter() {
                let nx = cx + dx;
                let ny = cy + dy;

                if !(nx >= 0 && nx < w && ny >= 0 && ny < h) {
                    continue;
                }

                if bd[ny as usize][nx as usize] & 1 != 0 {
                    continue;
                }

                if prev.contains_key(&(nx, ny)) {
                    continue;
                }

                prev.insert((nx, ny), (cx, cy));
                q.push_back((nx, ny));

                if bd[ny as usize][nx as usize] & 2 == 0
                    && if has_prios {
                        bd[ny as usize][nx as usize] & 4 != 0
                    } else {
                        true
                    }
                {
                    found = Some((nx, ny));
                    // dbg!((&found, bd[ny as usize][nx as usize]));
                    break;
                }
            }
            if found.is_some() {
                break;
            }
        }

        if found.is_none() {
            return None;
        }


        let (mut tx, mut ty) = found.unwrap();

        // let mut route_len = 0;

        loop {
            // route_len += 1;
            let n = prev[&(tx, ty)];
            if n == (x, y) {
                // dbg!((route_len, tx, ty, x, y));
                return Some((tx, ty));
            }
            tx = n.0;
            ty = n.1;
        }
    */

    let mut best_score = 0xffffffff;
    let mut ret = None;

    for (dx, dy) in VECT.iter() {
        let cx = x + dx;
        let cy = y + dy;

        if !(cx >= 0 && cx < w && cy >= 0 && cy < h) {
            continue;
        }

        if bd[cy as usize][cx as usize] & 1 != 0 {
            continue;
        }

        if let Some(dist) = nearest_empty_dist(bd, cx, cy, has_prios) {
            // dbg!((cx, cy, dist));
            if dist < best_score {
                best_score = dist;
                ret = Some((cx, cy));
            }
        }
    }


    ret
}

fn nearest_empty_dist(bd: &Vec<Vec<u8>>, x: i64, y: i64, has_prios: bool) -> Option<usize> {
    let h = bd.len() as i64;
    let w = bd[0].len() as i64;

    if bd[y as usize][x as usize] & 2 == 0
        && if has_prios {
            bd[y as usize][x as usize] & 4 != 0
        } else {
            true
        }
    {
        return Some(0);
    }


    let mut q = VecDeque::new();
    q.push_back((x, y, 0));

    let mut close = BTreeSet::<Pos>::new();
    close.insert((x, y));

    while let Some((cx, cy, dep)) = q.pop_front() {
        for &(dx, dy) in VECT.iter() {
            let nx = cx + dx;
            let ny = cy + dy;

            if !(nx >= 0 && nx < w && ny >= 0 && ny < h) {
                continue;
            }

            if bd[ny as usize][nx as usize] & 1 != 0 {
                continue;
            }

            if close.contains(&(nx, ny)) {
                continue;
            }

            close.insert((nx, ny));
            q.push_back((nx, ny, dep + 1));

            if bd[ny as usize][nx as usize] & 2 == 0
                && if has_prios {
                    bd[ny as usize][nx as usize] & 4 != 0
                } else {
                    true
                }
            {
                return Some(dep + 1);
            }
        }
    }

    None
}

struct State {
    bd: Vec<Vec<u8>>,
    x: i64,
    y: i64,
    items: Vec<usize>,
    manips: Vec<Pos>,
    rest: usize,
    prios: usize,

    hist: Vec<Diff>,
    diff: Diff,
}

#[derive(Default, Clone)]
struct Diff {
    bd: Vec<(usize, usize, u8)>,
    x: i64,
    y: i64,
    items: Vec<usize>,
    manips: Vec<Pos>,
    rest: usize,
    prios: usize,
}

impl State {
    fn new(bd: &Vec<Vec<u8>>, x: i64, y: i64) -> State {
        let mut ret = State {
            bd: bd.clone(),
            x,
            y,
            items: vec![0; 6 + 1],
            manips: vec![(0, 0), (1, 1), (1, 0), (1, -1)],
            rest: 0,
            prios: 0,

            hist: vec![],
            diff: Diff::default(),
        };
        ret.rest = ret.paint(x, y, true, false, 0, 1 << 30);
        ret.paint(x, y, false, false, 0, 1 << 30);
        ret.diff = ret.create_diff();
        ret
    }

    fn apply(&mut self, c: &Command) -> bool {
        let h = self.bd.len() as i64;
        let w = self.bd[0].len() as i64;

        match c {
            Command::Move(dx, dy) => {
                let x = self.x + dx;
                let y = self.y + dy;
                if !(x >= 0 && x < w && y >= 0 && y < h) {
                    return false;
                }
                if self.bd[y as usize][x as usize] & 1 != 0 {
                    return false;
                }
                self.move_to(x, y);
            }
            Command::Turn(dir) => self.rotate(*dir),

            _ => unreachable!(),
        }
        self.hist.push(self.diff.clone());
        self.diff = self.create_diff();
        true
    }

    fn unapply(&mut self) {
        let d = self.hist.pop().unwrap();
        self.apply_diff(d);
    }

    fn create_diff(&self) -> Diff {
        Diff {
            bd: vec![],
            x: self.x,
            y: self.y,
            items: self.items.clone(),
            manips: self.manips.clone(),
            rest: self.rest,
            prios: self.prios,
        }
    }

    fn apply_diff(&mut self, d: Diff) {
        self.x = d.x;
        self.y = d.x;
        self.items = d.items;
        self.manips = d.manips;
        self.rest = d.rest;
        self.rest = d.prios;

        for (x, y, v) in d.bd.into_iter().rev() {
            self.bd[y][x] = v;
        }
    }

    fn move_to(&mut self, x: i64, y: i64) {
        let h = self.bd.len() as i64;
        let w = self.bd[0].len() as i64;

        let mut mark_around = BTreeSet::new();

        for &(dx, dy) in self.manips.iter() {
            let tx = x + dx;
            let ty = y + dy;
            if !(tx >= 0 && tx < w && ty >= 0 && ty < h) {
                continue;
            }

            let mut ok = true;
            for (px, py) in pass_cells(x, y, tx, ty) {
                if self.bd[py as usize][px as usize] & 1 != 0 {
                    ok = false;
                    break;
                }
            }
            if !ok {
                continue;
            }

            let prev = self.bd[ty as usize][tx as usize];

            if self.bd[ty as usize][tx as usize] & 2 == 0 {
                self.rest -= 1;
            }
            self.bd[ty as usize][tx as usize] |= 2;

            if self.bd[ty as usize][tx as usize] & 4 != 0 {
                self.bd[ty as usize][tx as usize] &= !4;
                self.prios -= 1;
            }

            if self.bd[ty as usize][tx as usize] != prev {
                self.diff.bd.push((tx as usize, ty as usize, prev));
            }

            for &(dx, dy) in VECT.iter() {
                mark_around.insert((dx + tx, dy + ty));
            }
        }
        if self.bd[y as usize][x as usize] >> 4 != 0 {
            self.diff
                .bd
                .push((x as usize, y as usize, self.bd[y as usize][x as usize]));
            self.items[(self.bd[y as usize][x as usize] >> 4) as usize] += 1;
            self.bd[y as usize][x as usize] &= 0xf;
        }

        self.x = x;
        self.y = y;

        // mark islands
        for (mx, my) in mark_around {
            if !(mx >= 0 && mx < w && my >= 0 && my < h) {
                continue;
            }
            if self.bd[my as usize][mx as usize] & 4 != 0 {
                continue;
            }

            let island_size = self.paint(mx, my, true, false, 0, ISLAND_SIZE_THRESHOLD);
            self.paint(mx, my, false, false, 0, ISLAND_SIZE_THRESHOLD);
            if island_size < ISLAND_SIZE_THRESHOLD {
                self.paint(mx, my, true, true, 0, ISLAND_SIZE_THRESHOLD);
            }
        }
    }

    fn paint(&mut self, x: i64, y: i64, b: bool, diff: bool, mut acc: usize, th: usize) -> usize {
        let h = self.bd.len() as i64;
        let w = self.bd[0].len() as i64;

        if b && acc >= th {
            return acc;
        }

        let flg = if b { 0 } else { 4 };

        if !(x >= 0 && x < w && y >= 0 && y < h) {
            return acc;
        }

        if self.bd[y as usize][x as usize] & 1 != 0 {
            return acc;
        }

        if self.bd[y as usize][x as usize] & 2 != 0 {
            return acc;
        }

        if self.bd[y as usize][x as usize] & 4 != flg {
            return acc;
        }

        self.diff
            .bd
            .push((x as usize, y as usize, self.bd[y as usize][x as usize]));
        self.bd[y as usize][x as usize] ^= 4;
        if b {
            self.prios += 1;
        } else {
            self.prios -= 1;
        }

        acc += 1;
        acc = self.paint(x + 1, y, b, diff, acc, th);
        acc = self.paint(x - 1, y, b, diff, acc, th);
        acc = self.paint(x, y + 1, b, diff, acc, th);
        acc = self.paint(x, y - 1, b, diff, acc, th);
        acc
    }

    fn rotate(&mut self, dir: bool) {
        for r in self.manips.iter_mut() {
            swap(&mut r.0, &mut r.1);
            if dir {
                r.1 *= -1;
            } else {
                r.0 *= -1;
            }
        }
        self.move_to(self.x, self.y);
    }

    fn nearest_empty_dist(&self) -> usize {
        if self.rest == 0 {
            0
        } else {
            nearest_empty_dist(&self.bd, self.x, self.y, false).unwrap()
        }
    }

    fn score(&mut self) -> f64 {
        self.prios as f64 * 100.0 + self.rest as f64 + self.nearest_empty_dist() as f64 * 0.01
    }
}

fn pass_cells(x1: i64, y1: i64, x2: i64, y2: i64) -> Vec<(i64, i64)> {
    if x1 == x2 {
        (min(y1, y2)..=max(y1, y2)).map(|y| (x1, y)).collect()
    } else if x1 > x2 {
        pass_cells(x2, y2, x1, y1)
    } else {
        let mut ret = vec![];
        let grad = Rational::new((y2 - y1) as _, (x2 - x1) as _);
        let half = Rational::new(1, 2);
        for x in x1..=x2 {
            let l = max(
                Rational::from_integer(x1 as _),
                Rational::from_integer(x as _) - half,
            );
            let r = min(
                Rational::from_integer(x2 as _),
                Rational::from_integer(x as _) + half,
            );

            let ly: Rational = Rational::from_integer(y1 as _)
                + (l - Rational::from_integer(x1 as _)) * grad
                + half;
            let ry: Rational = Rational::from_integer(y1 as _)
                + (r - Rational::from_integer(x1 as _)) * grad
                + half;

            let lo = min(*ly.floor().numer(), *ry.floor().numer());
            let hi = max(*ly.ceil().numer(), *ry.ceil().numer());

            // dbg!((x, l, r, lo, hi));

            for y in lo..hi {
                ret.push((x, y as i64));
            }
        }
        ret
    }
}

#[test]
fn test_pass_cells() {
    assert_eq!(pass_cells(0, 0, -1, 1), vec![(-1, 1), (0, 0)]);
    assert_eq!(pass_cells(0, 0, 1, 1), vec![(0, 0), (1, 1)]);

    assert_eq!(
        pass_cells(0, 0, -2, 1),
        vec![(-2, 1), (-1, 0), (-1, 1), (0, 0)]
    );
    assert_eq!(
        pass_cells(0, 0, -3, 1),
        vec![(-3, 1), (-2, 1), (-1, 0), (0, 0)]
    );

    assert_eq!(pass_cells(0, 0, 1, 2), vec![(0, 0), (0, 1), (1, 1), (1, 2)]);
    assert_eq!(pass_cells(0, 0, 1, 3), vec![(0, 0), (0, 1), (1, 2), (1, 3)]);
}

fn solve(bd: &Vec<Vec<u8>>, sx: i64, sy: i64) -> Vec<Command> {
    let h = bd.len() as i64;
    let w = bd[0].len() as i64;

    let mut state = State::new(bd, sx, sy);
    let mut ret = vec![];
    state.move_to(sx, sy);

    loop {
        let n = nearest(&state.bd, state.x, state.y, state.prios > 0);
        if n.is_none() {
            break;
        }
        let (nx, ny) = n.unwrap();

        // 手が増やせるならとりあえず縦に増やす
        while state.items[1] > 0 {
            state.items[1] -= 1;

            for i in 0.. {
                let dy = if i % 2 == 0 { -2 - i / 2 } else { 2 + i / 2 };
                if !state.manips.contains(&(1, dy)) {
                    ret.push(Command::AttachManip(1, dy));
                    state.manips.push((1, dy));
                    break;
                }
            }
        }

        let dx = nx - state.x;
        let dy = ny - state.y;

        // dbg!((dx, dy, &state.manips));

        // 移動する方向にモップを回す
        // let mut rot_cnt = 0;
        // while !state.manips.contains(&(dx, dy)) {
        //     state.rotate(true);
        //     rot_cnt += 1;
        // }

        // if rot_cnt == 0 {
        // } else if rot_cnt == 1 {
        //     ret.push(Command::Turn(true));
        // } else if rot_cnt == 2 {
        //     ret.push(Command::Turn(true));
        //     ret.push(Command::Turn(true));
        // } else {
        //     ret.push(Command::Turn(false));
        // }

        // dbg!((rot_cnt, &state.manips));

        // 移動
        ret.push(Command::Move(dx, dy));
        state.move_to(nx, ny);

        // print_bd(&state.bd);
        // eprintln!("{}", &encode_commands(&ret));
    }

    ret
}

fn solve2(bd: &Vec<Vec<u8>>, sx: i64, sy: i64) -> Vec<Command> {
    let mut state = State::new(bd, sx, sy);
    let mut ret = vec![];

    let cand = vec![
        Command::Move(0, 1),
        Command::Move(0, -1),
        Command::Move(1, 0),
        Command::Move(-1, 0),
        Command::Turn(true),
        Command::Turn(false),
    ];

    while state.rest > 0 {
        let (_, cmd) = rec(&mut state, &cand, 10);
        state.apply(&cmd);
        ret.push(cmd);
        print_bd(&state.bd);
    }

    ret
}

fn rec(state: &mut State, cand: &[Command], rest: usize) -> (f64, Command) {
    if rest == 0 {
        return (state.score(), Command::Nop);
    }

    let mut ret = (1e100, Command::Nop);

    for c in cand.iter() {
        state.apply(c);
        let t = rec(state, cand, rest - 1);
        if t.0 < ret.0 {
            ret = t;
        }
        state.unapply();
    }

    ret
}

//-----

fn solve_lightning(name: &str, input: &Input, show_solution: bool) -> Result<()> {
    let mut input = input.clone();
    let (w, h) = normalize(&mut input);
    let mut bd = build_map(&input, w, h);
    let ans = solve(&mut bd, input.init_pos.0, input.init_pos.1);
    // print_ans(&ans);

    let score = ans.len() as i64;
    // eprintln!("Score: {}", score);

    let bs = get_best_score(LIGHTNING_DIR, name).unwrap();

    println!(
        "Score for {}: score = {}, best_score = {}",
        name,
        score,
        bs.unwrap_or(-1)
    );

    if show_solution {
        println!("{}", encode_commands(&ans));
    }
    save_solution(LIGHTNING_DIR, name, &ans, score)
}

//---------
// Storage

const LIGHTNING_DIR: &str = "results";
const FULL_DIR: &str = "results_full";

fn touch_dir<P: AsRef<Path>>(path: P) -> Result<()> {
    let path = path.as_ref();
    if !path.exists() {
        fs::create_dir_all(path)?;
    }
    Ok(())
}

fn get_result_dir(root: &str, name: &str) -> Result<PathBuf> {
    let pb = Path::new(root).join(name);
    touch_dir(&pb)?;
    Ok(pb)
}

fn get_problem_name<P: AsRef<Path>>(model: P) -> String {
    let re = Regex::new(r".*/prob-((\d|\w)+)\.desc").unwrap();
    let t = model.as_ref().to_string_lossy();
    let caps = re.captures(&t).unwrap();
    caps.get(1).unwrap().as_str().to_owned()
}

fn get_best_score(root: &str, name: &str) -> Result<Option<i64>> {
    let pb = get_result_dir(root, name)?;

    let re2 = Regex::new(r"/.+/(\d+)\.sol")?;
    let mut results: Vec<(i64, String)> = fs::read_dir(&pb)?
        .map(|e| {
            let s = e.unwrap().path().to_string_lossy().into_owned();
            let n = {
                let caps = re2.captures(&s).unwrap();
                caps.get(1).unwrap().as_str().parse().unwrap()
            };
            (n, s)
        })
        .collect();
    results.sort();

    Ok(if results.len() == 0 {
        None
    } else {
        Some(results[0].0)
    })
}

fn save_solution(root: &str, name: &str, ans: &[Command], score: i64) -> Result<()> {
    // println!("*****: {:?} {:?} {:?} {}", root, name, score);

    let best = get_best_score(root, name)?.unwrap_or(0);
    if best == 0 || best > score {
        println!("* Best score for {}: {} -> {}", name, best, score);
        let pb = get_result_dir(root, name)?;
        fs::write(pb.join(format!("{}.sol", score)), &encode_commands(ans))?;
    }
    Ok(())
}

//-----

fn main() -> Result<()> {
    match Opt::from_args() {
        Opt::Solve {
            input,
            show_solution,
        } => {
            let problem = read_input(&input)?;
            solve_lightning(&get_problem_name(&input), &problem, show_solution)?;
        }
        Opt::Pack => {
            let dir = LIGHTNING_DIR;

            let tmp = Path::new("tmp");
            if tmp.is_dir() {
                fs::remove_dir_all(tmp)?;
            }
            fs::create_dir_all(tmp)?;

            let mut files = vec![];

            let re = Regex::new(r"[a-z_]+/(.+)").unwrap();
            for entry in fs::read_dir(dir)? {
                let entry = entry?;
                let path = entry.path();
                let name = {
                    let t = path.to_string_lossy();
                    let caps = re.captures(&t).unwrap();
                    caps.get(1).unwrap().as_str().to_owned()
                };

                if let Some(best) = get_best_score(dir, &name)? {
                    let fname = format!("prob-{}.sol", name);
                    fs::copy(path.join(format!("{}.sol", best)), tmp.join(&fname))?;
                    files.push(fname);
                }
            }

            let dt = Local::now();
            let now = dt.format("%m%d%H%M");

            env::set_current_dir(tmp)?;
            std::process::Command::new("zip")
                .arg(format!("../submission-{}.zip", now))
                .args(&files)
                .status()?;
        }
    }
    Ok(())
}
