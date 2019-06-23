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

use std::str::FromStr;
use structopt::StructOpt;

mod puzzle;
use puzzle::*;

type Result<T> = std::result::Result<T, Box<std::error::Error>>;

// Board format
// bit 0: Wall
// bit 1: Painted
// bit 2: Portal
// bit 4-7: Booster
type Board = Vec<Vec<Cell>>;

#[derive(Clone, Copy, PartialEq, Eq)]
struct Cell(u16);

impl Cell {
    fn new(v: u16) -> Cell {
        Cell(v)
    }

    fn new_empty() -> Cell {
        Cell::new(0)
    }

    fn new_wall() -> Cell {
        Cell::new(1)
    }

    fn item(&self) -> Option<usize> {
        let t = (self.0 >> 4) as usize & 0xf;
        if t == 0 {
            None
        } else {
            Some(t)
        }
    }

    fn set_item(&mut self, item: Option<Booster>) {
        let c = item.map_or(0 as u16, booster2u16);
        self.0 = (self.0 & !(0xf << 4)) | (c << 4);
    }

    fn set_portal(&mut self) {
        self.0 = (self.0 | (1 << 2));
    }
    fn has_portal(&self) -> bool {
        self.0 & 4 != 0
    }

    fn is_wall(&self) -> bool {
        self.0 & 1 != 0
    }

    fn is_painted(&self) -> bool {
        self.0 & 2 != 0
    }

    fn set_painted(&mut self) {
        self.0 |= 2;
    }

    fn prio(&self) -> Option<usize> {
        let t = self.0 as usize >> 8;
        if t == 0 {
            None
        } else {
            Some(t - 1)
        }
    }

    fn set_prio(&mut self, prio: Option<usize>) {
        let c = prio.map_or(0, |v| v + 1) as u16;
        self.0 = (self.0 & !(0xff << 8)) | (c << 8);
    }

    fn to_char(&self) -> char {
        if (self.0 >> 4) & 0xf == 1 {
            'B'
        } else if (self.0 >> 4) & 0xf == 2 {
            'F'
        } else if (self.0 >> 4) & 0xf == 3 {
            'L'
        } else if (self.0 >> 4) & 0xf == 4 {
            'X'
        } else if (self.0 >> 4) & 0xf == 5 {
            'R'
        } else if (self.0 >> 4) & 0xf == 6 {
            'C'
        } else if self.0 >> 8 != 0 {
            (b'0' + (self.0 >> 8) as u8 - 1) as char
        } else if self.0 & 2 != 0 {
            '.'
        } else if self.0 & 1 != 0 {
            '#'
        } else {
            ' '
        }
    }
}

#[derive(Debug, StructOpt)]
enum Opt {
    #[structopt(name = "solve")]
    Solve {
        #[structopt(short = "s")]
        show_solution: bool,

        #[structopt(long = "increase-mop")]
        increase_mop: bool,

        #[structopt(short = "b")]
        bought_boosters: Option<String>,

        input: Option<PathBuf>,
    },

    #[structopt(name = "pack")]
    Pack,

    #[structopt(name = "solve-puzzle")]
    SolvePuzzle { input: Option<PathBuf> },
}

type Pos = (i64, i64);

const VECTS: &[&[Pos]] = &[
    &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    &[(1, 0), (0, -1), (-1, 0), (0, 1)],

    &[(-1, 0), (1, 0), (0, -1), (0, 1)],
    &[(1, 0), (-1, 0), (0, 1), (0, -1)],

    &[(-1, 0), (0, -1), (0, 1), (1, 0)],
    &[(1, 0), (0, 1), (0, -1), (-1, 0)],

    &[(-1, 0), (0, 1), (0, -1), (1, 0)],
    &[(1, 0), (0, -1), (0, 1), (-1, 0)],

    &[(-1, 0), (1, 0), (0, 1), (0, -1)],
    &[(1, 0), (-1, 0), (0, -1), (0, 1)],

    &[(-1, 0), (0, -1), (1, 0), (0, 1)],
    &[(1, 0), (0, 1), (-1, 0), (0, -1)],

    // &[(0, -1), (1, 0), (0, 1), (-1, 0)],
    // &[(0, 1), (-1, 0), (0, -1), (1, 0)],
    // &[(1, 0), (0, -1), (-1, 0), (0, 1)],
    // &[(0, -1), (1, 0), (0, 1), (-1, 0)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(1, 0), (0, -1), (-1, 0), (0, 1)],
    // &[(0, -1), (1, 0), (0, 1), (-1, 0)],

    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    // &[(-1, 0), (0, 1), (1, 0), (0, -1)],
];
// const VECT: &[Pos] = &[(-1, 0), (0, 1), (1, 0), (0, -1)];

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
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
    SetPortal,
    Teleport(i64, i64),
    Clone,
}

type Solution = Vec<Vec<Command>>;

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

fn char2booster(c: char) -> Booster {
    match c {
        'B' => Booster::B,
        'F' => Booster::F,
        'L' => Booster::L,
        'X' => Booster::X,
        'R' => Booster::R,
        'C' => Booster::C,
        _ => unreachable!()
    }
}

fn booster2u16(typ: Booster) -> u16 {
    (typ as u16) + 1
}

fn parse_booster(s: &str) -> Result<(Booster, Pos)> {
    let ty = char2booster(s.chars().next().unwrap());
    let pos = parse_pos(&s[1..])?;
    Ok((ty, pos))
}

fn parse_booster_list(s: &str) -> Result<Vec<(Booster, Pos)>> {
    s.split(';')
        .filter(|w| w.trim() != "")
        .map(parse_booster)
        .collect()
}

fn parse_input(s: &str) -> Result<Input> {
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

fn print_bd(bd: &Board) {
    for y in (0..bd.len()).rev() {
        for x in 0..bd[y].len() {
            eprint!("{}", bd[y][x].to_char());
        }
        eprintln!();
    }
    eprintln!();
}

fn encode_commands(ans: &Solution) -> String {
    let mut ret = String::new();
    let num = ans.iter().map(|r| r.len()).max().unwrap();

    for i in 0..num {
        if i > 0 {
            ret += "#";
        }
        for cmds in ans.iter() {
            if i < cmds.len() {
                let cmd = &cmds[i];
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
                    Command::SetPortal => ret += "R",
                    Command::Teleport(x, y) => ret += &format!("T({},{})", x, y),
                    Command::Clone => ret += "C",
                }
            }
        }
    }
    ret
}

fn build_map(input: &Input, w: i64, h: i64) -> Board {
    let mut bd = vec![vec![Cell::new_wall(); w as usize]; h as usize];
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
                bd[y as usize][j as usize] = Cell::new_empty();
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
                bd[y as usize][j as usize] = Cell::new_wall();
            }
        }
    }

    for (typ, (x, y)) in input.boosters.iter() {
        bd[*y as usize][*x as usize].set_item(Some(*typ));
    }

    bd
}

// solution

fn nearest(state: &State, i: usize, f: impl Fn(Cell) -> bool + Copy) -> Option<(i64, i64)> {
    let h = state.bd.len() as i64;
    let w = state.bd[0].len() as i64;

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

    let mut best_score = (0xffffffff, 0xffffffff);
    let mut ret = None;

    for &(dx, dy) in VECTS[i % VECTS.len()].iter() {
        let cx = state.robots[i].x + dx;
        let cy = state.robots[i].y + dy;

        if !(cx >= 0 && cx < w && cy >= 0 && cy < h) {
            continue;
        }

        if state.bd[cy as usize][cx as usize].is_wall() {
            continue;
        }

        if let Some(dist) = nearest_empty_dist(&state.bd, cx, cy, f, VECTS[i % VECTS.len()]) {
            //let sc = state.try_move_to(cx, cy, i);
            let sc = 0_i64;
            if (dist, -sc) < best_score {
                best_score = (dist, -sc);
                ret = Some((cx, cy));
            }
        }
    }

    for &(x, y) in state.portals.iter() {
        if let Some(dist) = nearest_empty_dist(&state.bd, x, y, f, VECTS[i]) {
            let sc = 0_i64;
            if (dist, -sc) < best_score {
                best_score = (dist, -sc);
                ret = Some((x, y));
            }
        }
    }

    ret
}

fn nearest_empty_dist(
    bd: &Board,
    x: i64,
    y: i64,
    f: impl Fn(Cell) -> bool,
    vect: &[Pos],
) -> Option<usize> {
    let h = bd.len() as i64;
    let w = bd[0].len() as i64;

    if f(bd[y as usize][x as usize]) {
        return Some(0);
    }

    let mut q = VecDeque::new();
    q.push_back((x, y, 0));

    let mut close = BTreeSet::<Pos>::new();
    close.insert((x, y));

    while let Some((cx, cy, dep)) = q.pop_front() {
        for &(dx, dy) in vect.iter() {
            let nx = cx + dx;
            let ny = cy + dy;

            if !(nx >= 0 && nx < w && ny >= 0 && ny < h) {
                continue;
            }

            if bd[ny as usize][nx as usize].is_wall() {
                continue;
            }

            if close.contains(&(nx, ny)) {
                continue;
            }

            close.insert((nx, ny));
            q.push_back((nx, ny, dep + 1));

            if f(bd[ny as usize][nx as usize]) {
                return Some(dep + 1);
            }
        }
    }

    None
}

struct RobotState {
    x: i64,
    y: i64,
    manips: Vec<Pos>,
    prios: usize,
    num_collected_portal: usize,
}

struct State {
    bd: Board,
    robots: Vec<RobotState>,
    items: Vec<usize>,
    rest: usize,
    clone_num: usize,
    portals: Vec<Pos>,

    hist: Vec<Diff>,
    diff: Diff,
    island_size_threshold: i64,
}

#[derive(Default, Clone)]
struct Diff {
    bd: Vec<(usize, usize, Cell)>,
    x: i64,
    y: i64,
    items: Vec<i64>,
    manips: Vec<Pos>,
    rest: usize,
}

impl State {
    fn new(bd: &Board, x: i64, y: i64, island_size_threshold: i64, bought_boosters: &str) -> State {
        let mut items = vec![0; 6 + 1];
        for c in bought_boosters.chars() {
            items[booster2u16(char2booster(c)) as usize] += 1;
        }
        let mut clone_num = 0;
        for y in 0..bd.len() {
            for x in 0..bd[y].len() {
                if bd[y][x].item() == Some(6) {
                    clone_num += 1;
                }
            }
        }
        let mut has_mystery = false;
        for y in 0..bd.len() {
            for x in 0..bd[y].len() {
                if bd[y][x].item() == Some(4) {
                    has_mystery = true;
                    break;
                }
            }
        }
        if !has_mystery {
            clone_num = 0;
        }

        let mut ret = State {
            bd: bd.clone(),
            robots: vec![RobotState {
                x,
                y,
                manips: vec![(0, 0), (1, 1), (1, 0), (1, -1)],
                prios: 0,
                num_collected_portal: 0,
            }],
            items: items,
            rest: 0,
            clone_num,
            portals: vec![],

            hist: vec![],
            diff: Diff::default(),
            island_size_threshold: island_size_threshold,
        };
        ret.rest = ret.paint(x, y, 0, true, false, 0, 1 << 30);
        ret.paint(x, y, 0, false, false, 0, 1 << 30);
        // ret.diff = ret.create_diff();
        ret
    }

    fn add_robot(&mut self, x: i64, y: i64) {
        self.robots.push(RobotState {
            x,
            y,
            manips: vec![(0, 0), (1, 1), (1, 0), (1, -1)],
            prios: 0,
            num_collected_portal: 0,
        });
    }

    fn apply(&mut self, c: &Command) -> bool {
        unimplemented!()
        // let h = self.bd.len() as i64;
        // let w = self.bd[0].len() as i64;

        // match c {
        //     Command::Move(dx, dy) => {
        //         let x = self.x + dx;
        //         let y = self.y + dy;
        //         if !(x >= 0 && x < w && y >= 0 && y < h) {
        //             return false;
        //         }
        //         if self.bd[y as usize][x as usize] & 1 != 0 {
        //             return false;
        //         }
        //         self.move_to(x, y);
        //     }
        //     Command::Turn(dir) => self.rotate(*dir),

        //     _ => unreachable!(),
        // }
        // self.hist.push(self.diff.clone());
        // self.diff = self.create_diff();
        // true
    }

    fn unapply(&mut self) {
        let d = self.hist.pop().unwrap();
        self.apply_diff(d);
    }

    fn create_diff(&self) -> Diff {
        // Diff {
        //     bd: vec![],
        //     x: self.x,
        //     y: self.y,
        //     items: self.items.clone(),
        //     manips: self.manips.clone(),
        //     rest: self.rest,
        //     prios: self.prios,
        // }
        unimplemented!()
    }

    fn apply_diff(&mut self, d: Diff) {
        // self.x = d.x;
        // self.y = d.x;
        // self.items = d.items;
        // self.manips = d.manips;
        // self.rest = d.rest;
        // self.rest = d.prios;

        // for (x, y, v) in d.bd.into_iter().rev() {
        //     self.bd[y][x] = v;
        // }
        unimplemented!()
    }

    fn move_to(&mut self, x: i64, y: i64, i: usize, detect_island: bool) {
        let h = self.bd.len() as i64;
        let w = self.bd[0].len() as i64;

        let mut mark_around = BTreeSet::new();

        for (dx, dy) in self.robots[i].manips.clone() {
            let tx = x + dx;
            let ty = y + dy;
            if !(tx >= 0 && tx < w && ty >= 0 && ty < h) {
                continue;
            }

            let mut ok = true;
            for (px, py) in pass_cells(x, y, tx, ty) {
                if self.bd[py as usize][px as usize].is_wall() {
                    ok = false;
                    break;
                }
            }
            if !ok {
                continue;
            }

            let prev = self.bd[ty as usize][tx as usize];

            if !self.bd[ty as usize][tx as usize].is_painted() {
                self.rest -= 1;
            }
            self.bd[ty as usize][tx as usize].set_painted();

            let prio = self.bd[ty as usize][tx as usize].prio();

            if let Some(prio) = prio {
                self.bd[ty as usize][tx as usize].set_prio(None);
                self.robots[prio].prios -= 1;

                // dbg!("*** CLEANUP PRIORITY");
            }

            if self.bd[ty as usize][tx as usize] != prev {
                self.diff.bd.push((tx as usize, ty as usize, prev));
            }

            for &(dx, dy) in VECTS[i % VECTS.len()].iter() {
                mark_around.insert((dx + tx, dy + ty));
            }
        }

        self.robots[i].x = x;
        self.robots[i].y = y;

        // mark islands
        if detect_island {
            for (mx, my) in mark_around {
                if !(mx >= 0 && mx < w && my >= 0 && my < h) {
                    continue;
                }
                if self.bd[my as usize][mx as usize].prio() != None {
                    continue;
                }

                let island_size = self.paint(
                    mx,
                    my,
                    i,
                    true,
                    false,
                    0,
                    self.island_size_threshold as usize,
                );
                self.paint(
                    mx,
                    my,
                    i,
                    false,
                    false,
                    0,
                    self.island_size_threshold as usize,
                );
                if island_size < self.island_size_threshold as usize {
                    self.paint(
                        mx,
                        my,
                        i,
                        true,
                        true,
                        0,
                        self.island_size_threshold as usize,
                    );
                }
            }
        }
    }

    // fn try_move_to(&self, x: i64, y: i64, i: usize) -> i64 {
    //     let mut score = 0;

    //     let h = self.bd.len() as i64;
    //     let w = self.bd[0].len() as i64;

    //     for &(dx, dy) in self.robots[i].manips.iter() {
    //         let tx = x + dx;
    //         let ty = y + dy;
    //         if !(tx >= 0 && tx < w && ty >= 0 && ty < h) {
    //             continue;
    //         }

    //         let mut ok = true;
    //         for (px, py) in pass_cells(x, y, tx, ty) {
    //             if self.bd[py as usize][px as usize] & 1 != 0 {
    //                 ok = false;
    //                 break;
    //             }
    //         }
    //         if !ok {
    //             continue;
    //         }

    //         let prev = self.bd[ty as usize][tx as usize];

    //         if self.bd[ty as usize][tx as usize] & 2 == 0 {
    //             score += 1;
    //         }
    //     }
    //     if self.bd[y as usize][x as usize] >> 4 != 0 {
    //         score += 2;
    //     }
    //     score
    // }

    fn paint(
        &mut self,
        x: i64,
        y: i64,
        i: usize,
        b: bool,
        diff: bool,
        mut acc: usize,
        th: usize,
    ) -> usize {
        let h = self.bd.len() as i64;
        let w = self.bd[0].len() as i64;

        if b && acc >= th {
            return acc;
        }

        if !(x >= 0 && x < w && y >= 0 && y < h) {
            return acc;
        }

        let cell = self.bd[y as usize][x as usize];

        if cell.is_wall() {
            return acc;
        }

        if cell.is_painted() {
            return acc;
        }

        if if b {
            cell.prio() != None
        } else {
            cell.prio() != Some(i)
        } {
            return acc;
        }

        if diff {
            self.diff.bd.push((x as usize, y as usize, cell));
        }

        if b {
            self.bd[y as usize][x as usize].set_prio(Some(i));
            self.robots[i].prios += 1;
        } else {
            self.bd[y as usize][x as usize].set_prio(None);
            self.robots[i].prios -= 1;
        }

        acc += 1;
        acc = self.paint(x + 1, y, i, b, diff, acc, th);
        acc = self.paint(x - 1, y, i, b, diff, acc, th);
        acc = self.paint(x, y + 1, i, b, diff, acc, th);
        acc = self.paint(x, y - 1, i, b, diff, acc, th);
        acc
    }

    fn rotate(&mut self, dir: bool, i: usize) {
        for r in self.robots[i].manips.iter_mut() {
            swap(&mut r.0, &mut r.1);
            if dir {
                r.1 *= -1;
            } else {
                r.0 *= -1;
            }
        }
        self.move_to(self.robots[i].x, self.robots[i].y, i, true);
    }

    fn nearest_empty_dist(&self, i: usize, f: impl Fn(Cell) -> bool) -> usize {
        if self.rest == 0 {
            0
        } else {
            nearest_empty_dist(
                &self.bd,
                self.robots[i].x,
                self.robots[i].y,
                f,
                VECTS[i % VECTS.len()],
            )
            .unwrap()
        }
    }

    // fn score(&mut self, i: usize) -> f64 {
    //     self.prios as f64 * 100.0 + self.rest as f64 + self.nearest_empty_dist(i) as f64 * 0.01
    // }

    fn dump(&self) {
        for i in 0..self.robots.len() {
            eprintln!("Roboto[{}]: {}, {}", i, self.robots[i].x, self.robots[i].y);
        }
        eprintln!("Items: {:?}", self.items);
        print_bd(&self.bd);
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

fn solve(
    bd_org: &Board,
    sx: i64,
    sy: i64,
    island_size_threshold: i64,
    aggressive_item: bool,
    increase_mop: bool,
    bought_boosters: &str,
) -> Solution {
    let h = bd_org.len() as i64;
    let w = bd_org[0].len() as i64;

    let mut state = State::new(bd_org, sx, sy, island_size_threshold, bought_boosters);
    let mut ret: Solution = vec![];
    state.move_to(sx, sy, 0, false);

    // state.dump();

    let mut fin = false;

    while !fin {
        let mut cmds = vec![];
        let robot_num = state.robots.len();

        let shortest_mop = state
            .robots
            .iter()
            .enumerate()
            .map(|(i, r)| (r.manips.len(), i))
            .min()
            .unwrap()
            .1;

        // eprintln!("### TURN");

        for i in 0..robot_num {
            // アイテム取得処理
            if let Some(item) =
                state.bd[state.robots[i].y as usize][state.robots[i].x as usize].item() {
                if item != 4 {  // Not X.
                    state.diff
                        .bd
                        .push((state.robots[i].x as usize, state.robots[i].y as usize, state.bd[state.robots[i].y as usize][state.robots[i].x as usize]));
                    state.items[item] += 1;
                    state.bd[state.robots[i].y as usize][state.robots[i].x as usize].set_item(None);

                    // note: clone_numはXがいないと0になってる。
                    if item == 6 && state.clone_num > 0 {
                        state.clone_num -= 1;
                    }

                    if item == 5 {
                        state.robots[i].num_collected_portal += 1;
                    }
                }
            }

            if i == 0 {
                if state.items[6] > 0 {
                    if state.bd[state.robots[i].y as usize][state.robots[i].x as usize].item()
                        == Some(4)
                    {
                        // eprintln!("*****CLONE*****");

                        cmds.push(Command::Clone);
                        state.add_robot(state.robots[i].x, state.robots[i].y);
                        state.items[6] -= 1;
                    } else {
                        // if nearest(&state, i, |c| (c >> 4) == 4).is_none() {
                        //     state.dump();
                        // }

                        let (nx, ny) = nearest(&state, i, |c| c.item() == Some(4)).unwrap();
                        let dx = nx - state.robots[i].x;
                        let dy = ny - state.robots[i].y;
                        if dx.abs() + dy.abs() == 1 {
                            cmds.push(Command::Move(dx, dy));
                        } else {
                            cmds.push(Command::Teleport(nx, ny));
                        }
                        state.move_to(nx, ny, i, false);
                    }
                    continue;
                } else if state.clone_num > 0 {
                    // if nearest(&state, i, |c| (c >> 4) & 0xf == 6).is_none() {
                    //     state.dump();
                    // }

                    // eprintln!("***** CLONE_NUM: {} *****", state.clone_num);
                    let (nx, ny) = nearest(&state, i, |c| c.item() == Some(6)).unwrap();
                    let dx = nx - state.robots[i].x;
                    let dy = ny - state.robots[i].y;
                    if dx.abs() + dy.abs() == 1 {
                        cmds.push(Command::Move(dx, dy));
                    } else {
                        cmds.push(Command::Teleport(nx, ny));
                    }
                    state.move_to(nx, ny, i, false);
                    continue;
                }
            }

            if aggressive_item {
                let mut done = false;
                // 隣にアイテムがあったら拾う
                for &(dx, dy) in VECTS[i % VECTS.len()].iter() {
                    let cx = dx + state.robots[i].x;
                    let cy = dy + state.robots[i].y;
                    if !(cx >= 0 && cx < w && cy >= 0 && cy < h) {
                        continue;
                    }
                    let item = state.bd[cy as usize][cx as usize].item();
                    // 使える奴だけ
                    if item == Some(1) || item == Some(5) {
                        // eprintln!("{} {} -> {} {}", state.x, state.y, cx, cy);
                        // eprintln!("{}, item: {}", state.bd[cy as usize][cx as usize], item);
                        state.move_to(cx, cy, i, true);
                        cmds.push(Command::Move(dx, dy));
                        // eprintln!("{}", state.bd[cy as usize][cx as usize]);
                        done = true;
                        break;
                    }
                }
                if done {
                    // state.dump();
                    continue;
                }
            }

            if state.robots[i].num_collected_portal > 0 &&
                state.items[5] > 0 &&
                state.bd[state.robots[i].y as usize][state.robots[i].x as usize].item() != Some(4) &&
                !state.bd[state.robots[i].y as usize][state.robots[i].x as usize].has_portal() {
                state.robots[i].num_collected_portal -= 1;
                state.portals.push((state.robots[i].x, state.robots[i].y));
                state.bd[state.robots[i].y as usize][state.robots[i].x as usize].set_portal();
                cmds.push(Command::SetPortal);
                continue;
            }

            let n = nearest(&state, i, |c| {
                !c.is_painted()
                    && if state.robots[i].prios > 0 {
                        c.prio() == Some(i)
                    } else {
                        true
                    }
            });
            if n.is_none() {
                if i == 0 {
                    return ret;
                }
                fin = true;
                cmds.push(Command::Nop);
                continue;
            }
            let (nx, ny) = n.unwrap();

            // 手が増やせるならとりあえず縦に増やす
            if increase_mop {
                if state.items[1] > 0 && i == shortest_mop {
                    state.items[1] -= 1;

                    for ii in 0.. {
                        let dy = if ii % 2 == 0 { -2 - ii / 2 } else { 2 + ii / 2 };
                        if !state.robots[i].manips.contains(&(1, dy)) {
                            // eprintln!("**** ADD ARM ****");
                            cmds.push(Command::AttachManip(1, dy));
                            state.robots[i].manips.push((1, dy));
                            break;
                        }
                    }
                    continue;
                }
            }

            let dx = nx - state.robots[i].x;
            let dy = ny - state.robots[i].y;

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
            if dx.abs() + dy.abs() == 1 {
                cmds.push(Command::Move(dx, dy));
            } else {
                cmds.push(Command::Teleport(nx, ny));
            }
            state.move_to(nx, ny, i, true);

            // eprintln!("{}", &encode_commands(&ret));
        }

        // state.dump();
        ret.push(cmds);
    }

    ret
}

// fn solve2(bd: &Board, sx: i64, sy: i64) -> Vec<Command> {
//     let mut state = State::new(bd, sx, sy, 50);
//     let mut ret = vec![];

//     let cand = vec![
//         Command::Move(0, 1),
//         Command::Move(0, -1),
//         Command::Move(1, 0),
//         Command::Move(-1, 0),
//         Command::Turn(true),
//         Command::Turn(false),
//     ];

//     while state.rest > 0 {
//         let (_, cmd) = rec(&mut state, &cand, 10);
//         state.apply(&cmd);
//         ret.push(cmd);
//         print_bd(&state.bd);
//     }

//     ret
// }

// fn rec(state: &mut State, cand: &[Command], rest: usize) -> (f64, Command) {
//     if rest == 0 {
//         return (state.score(), Command::Nop);
//     }

//     let mut ret = (1e100, Command::Nop);

//     for c in cand.iter() {
//         state.apply(c);
//         let t = rec(state, cand, rest - 1);
//         if t.0 < ret.0 {
//             ret = t;
//         }
//         state.unapply();
//     }

//     ret
// }

//-----

fn solve_lightning(
    name: &str,
    input: &Input,
    increase_mop: bool,
    show_solution: bool,
    bought_boosters: &str,
) -> Result<()> {
    let mut input = input.clone();
    let (w, h) = normalize(&mut input);
    let mut bd = build_map(&input, w, h);

    let mut ans = vec![];

    // let island_th_params = 0..20;
    let island_th_params = 9..10;

    for i in island_th_params {
        let aggressive_item_get = 1..2;

        let th = 5 + 5 * i;
        for j in aggressive_item_get {
            let cur = solve(
                &mut bd,
                input.init_pos.0,
                input.init_pos.1,
                th,
                j != 0,
                increase_mop,
                bought_boosters,
            );
            eprintln!("{} {}: {}", i, j, cur.len());
            if ans.len() == 0 || ans.len() > cur.len() {
                ans = cur;
            }
        }
    }

    let score = ans.len() as i64;
    // eprintln!("Score: {}", score);

    if name != "stdin" {
        let bs = get_best_score(LIGHTNING_DIR, name).unwrap();

        eprintln!(
            "Score for {}: score = {}, best_score = {}",
            name,
            score,
            bs.unwrap_or(-1)
        );
    }

    if show_solution || name == "stdin" {
        println!("{}", encode_commands(&ans));
    } else {
        save_solution(LIGHTNING_DIR, name, &ans, score)?;
    }

    Ok(())
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

fn save_solution(root: &str, name: &str, ans: &Solution, score: i64) -> Result<()> {
    // println!("*****: {:?} {:?} {:?} {}", root, name, score);

    let best = get_best_score(root, name)?.unwrap_or(0);
    if best == 0 || best > score {
        eprintln!("* Best score for {}: {} -> {}", name, best, score);
        let pb = get_result_dir(root, name)?;
        fs::write(pb.join(format!("{}.sol", score)), &encode_commands(ans))?;
    }
    Ok(())
}

//-----

fn read_file(path: &Option<PathBuf>) -> Result<(String, PathBuf)> {
    Ok(if let Some(file) = path {
        let mut s = String::new();
        let _ = File::open(&file)?.read_to_string(&mut s)?;
        (s.trim().to_string(), file.clone())
    } else {
        let mut s = String::new();
        let _ = std::io::stdin().read_to_string(&mut s)?;
        (
            s.trim().to_string(),
            PathBuf::from_str("./prob-stdin.desc")?,
        )
    })
}

fn main() -> Result<()> {
    match Opt::from_args() {
        Opt::Solve {
            input,
            increase_mop,
            bought_boosters,
            show_solution,
        } => {
            let (con, file) = read_file(&input)?;
            let problem = parse_input(&con)?;
            solve_lightning(
                &get_problem_name(&file),
                &problem,
                increase_mop,
                show_solution,
                &bought_boosters.unwrap_or_default(),
            )?;
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

        Opt::SolvePuzzle { input } => {
            let (con, _) = read_file(&input)?;
            solve_puzzle(&parse_puzzle(&con)?);
        }
    }
    Ok(())
}
