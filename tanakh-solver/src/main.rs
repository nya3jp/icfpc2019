use chrono::prelude::*;
use num_rational::Rational;
use regex::Regex;
use std::cmp::{max, min};
use std::collections::{BTreeSet, HashMap, HashSet, VecDeque};
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

#[derive(Debug, StructOpt)]
struct SolverOption {
    /// 島のサイズの上限
    #[structopt(long = "island-threshold", default_value = "50")]
    island_size_threshold: i64,

    /// モップのサイズを増やすかどうか
    #[structopt(short = "A", long = "increase-mop")]
    increase_mop: bool,

    /// クローンされたやつがいろんな方向向くやつ
    #[structopt(short = "B", long = "change-clone-dir")]
    change_clone_dir: bool,

    /// 隣接したアイテムをスルーせずにとる（使えるものだけ）
    #[structopt(short = "C", long = "aggressive-item")]
    aggressive_item: bool,

    /// ポータルをクローンの次に優先して取りに行く
    #[structopt(short = "D", long = "aggressive-teleport")]
    aggressive_teleport: bool,

    /// ドリルを使う
    #[structopt(short = "E", long = "use-drill")]
    use_drill: bool,

    /// 0 以外も C を取りに行く && 産み落とす
    #[structopt(long = "spawn-delegate")]
    spawn_delegate: bool,

    /// 取ったら即座に F を使う
    #[structopt(long = "aggressive-fast")]
    aggressive_fast: bool,

    /// モップを向ける
    #[structopt(long = "mop-direction")]
    mop_direction: bool,

    /// 0 以外の bot が C を回収する
    #[structopt(long = "aggressive-c-collect")]
    aggressive_c_collect: bool,

    /// Cloneしない
    #[structopt(long = "disable-clone")]
    disable_clone: bool,

    // Randome に portal を drop する
    #[structopt(long = "drop-portal-randomly")]
    drop_portal_randomly: bool,
}

#[derive(Debug, StructOpt)]
enum Opt {
    #[structopt(name = "solve")]
    Solve {
        #[structopt(short = "s")]
        show_solution: bool,

        #[structopt(flatten)]
        solver_option: SolverOption,

        #[structopt(short = "f")]
        force_save: bool,

        #[structopt(short = "b", default_value = "")]
        bought_boosters: String,

        input: Option<PathBuf>,
    },

    #[structopt(name = "pack")]
    Pack,

    #[structopt(name = "solve-puzzle")]
    SolvePuzzle { input: Option<PathBuf> },
}

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
        self.0 = self.0 | (1 << 2);
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

type Pos = (i64, i64);

const VECTS1: &[&[Pos]] = &[
    &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    &[(1, 0), (0, 1), (-1, 0), (0, -1)],
    &[(-1, 0), (0, 1), (0, -1), (1, 0)],
    &[(1, 0), (-1, 0), (0, 1), (0, -1)],
    &[(-1, 0), (1, 0), (0, 1), (0, -1)],
    &[(1, 0), (0, -1), (0, 1), (-1, 0)],

    &[(-1, 0), (0, 1), (1, 0), (0, -1)],
    &[(0, -1), (1, 0), (0, 1), (-1, 0)],
    &[(1, 0), (0, -1), (-1, 0), (0, 1)],
    &[(0, 1), (-1, 0), (0, -1), (1, 0)],

    &[(-1, 0), (0, -1), (1, 0), (0, 1)],
    &[(0, 1), (1, 0), (0, -1), (-1, 0)],
    &[(1, 0), (0, 1), (-1, 0), (0, -1)],
    &[(0, -1), (1, 0), (0, 1), (-1, 0)],
];

const VECTS2: &[&[Pos]] = &[
    &[(-1, 0), (0, -1), (0, 1), (1, 0)],
    &[(1, 0), (0, 1), (0, -1), (-1, 0)],

    &[(-1, 0), (0, 1), (0, -1), (1, 0)],
    &[(1, 0), (0, -1), (0, 1), (-1, 0)],

    &[(-1, 0), (1, 0), (0, 1), (0, -1)],
    &[(1, 0), (-1, 0), (0, -1), (0, 1)],

    &[(-1, 0), (0, -1), (1, 0), (0, 1)],
    &[(1, 0), (0, 1), (-1, 0), (0, -1)],
];

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

// const VECT: &[Pos] = &[(-1, 0), (0, 1), (1, 0), (0, -1)];

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
enum Booster {
    B = 1,
    F,
    L,
    X,
    R,
    C,
}

fn char2booster(c: char) -> Booster {
    match c {
        'B' => Booster::B,
        'F' => Booster::F,
        'L' => Booster::L,
        'X' => Booster::X,
        'R' => Booster::R,
        'C' => Booster::C,
        _ => unreachable!(),
    }
}

fn booster2u16(typ: Booster) -> u16 {
    typ as u16
}

#[derive(Debug, Clone, PartialEq)]
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
    /*
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
    */

    (max_x, max_y)
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

/*
fn nearest_cmd(state: &State, i: usize) -> Option<Command> {
    let h = state.bd.len() as i64;
    let w = state.bd[0].len() as i64;

    type St = (i64, i64, usize);

    let mut q = VecDeque::new();
    q.push_back((state.robots[i].x, state.robots[i].y, 0));

    let mut found = None;
    let mut prev = BTreeMap::<St, St>::new();

    const MOVS: [(i64, i64, usize)] = [
        (0, 0, 1),
        (0, 0, 3),
        (1, 0, 0),
        (-1, 0, 0),
        (0, 1, 0),
        (0, -1, 0),
    ];

    while let Some((cx, cy, cdir)) = q.pop_front() {
        for &(dx, dy, ddir) in moves.iter() {
            let nx = cx + dx;
            let ny = cy + dy;
            let ndir = (cdir + )

            if !(nx >= 0 && nx < w && ny >= 0 && ny < h) {
                continue;
            }

            if bd[ny as usize][nx as usize] & 1 != 0 {
                continue;
            }

            if prev.contains_key(&(nx, ny, ndir)) {
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
    ret
}
*/

fn nearest(
    state: &State,
    i: usize,
    f: impl Fn(i64, i64, Cell) -> bool + Copy,
) -> Option<(Pos, usize, Pos)> {
    let h = state.bd.len() as i64;
    let w = state.bd[0].len() as i64;

    let orig_x = state.robots[i].x;
    let orig_y = state.robots[i].y;
    let bd = &state.bd;

    if f(orig_x, orig_y, bd[orig_y as usize][orig_x as usize]) {
        return Some(((orig_x, orig_y), 0, (orig_x, orig_y)));
    }

    let mut q = VecDeque::new();
    let mut backtrack = HashMap::new();
    let mut cands = vec![];

    // Initialize starting set.
    let vect = &state.robots[i].vect;

    for (ix, &(dx, dy)) in vect.iter().enumerate() {
        let cx = orig_x + dx;
        let cy = orig_y + dy;

        if cx < 0 || w <= cx || cy < 0 || h <= cy {
            continue;
        }
        if bd[cy as usize][cx as usize].is_wall() {
            continue;
        }

        let sc = state.overwrap_score(cx, cy, i);
        cands.push(((sc, ix), (cx, cy)));
    }

    // Portals.
    for (ix, &(x, y)) in state.portals.iter().enumerate() {
        let sc = state.overwrap_score(x, y, i);
        cands.push(((sc, ix + vect.len()), (x, y)));
    }

    // ordering
    cands.sort();

    for (_, (x, y)) in cands {
        // q.push_back((cx, cy));
        // backtrack.insert((cx, cy), (orig_x, orig_y));

        q.push_back((x, y));
        backtrack.insert((x, y), (orig_x, orig_y));
    }

    let found = loop {
        if q.is_empty() {
            break None;
        }
        let (cx, cy) = q.pop_front().unwrap();
        if f(cx, cy, bd[cy as usize][cx as usize]) {
            break Some((cx, cy));
        }

        for &(dx, dy) in vect.iter() {
            let nx = cx + dx;
            let ny = cy + dy;

            if nx < 0 || w <= nx || ny < 0 || h <= ny {
                // Out of map.
                continue;
            }
            if bd[ny as usize][nx as usize].is_wall() {
                continue;
            }
            if backtrack.contains_key(&(nx, ny)) {
                // Already visited.
                continue;
            }
            backtrack.insert((nx, ny), (cx, cy));
            q.push_back((nx, ny));
        }
    };

    if found.is_none() {
        return None;
    }

    let (gx, gy) = found.unwrap();
    let (mut cx, mut cy) = found.unwrap();
    let mut dist = 1;
    loop {
        match backtrack.get(&(cx, cy)) {
            Some(&(nx, ny)) => {
                if nx == orig_x && ny == orig_y {
                    return Some(((cx, cy), dist, (gx, gy)));
                }
                cx = nx;
                cy = ny;
                dist += 1;
            }
            None => unreachable!(),
        }
    }
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

    let mut close = HashSet::<Pos>::new();
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
    drill_time: usize,
    queue: Vec<Command>,
    vect: Vec<Pos>,
    fast_count: usize,
    target: Option<Pos>,
}

struct State {
    bd: Board,
    robots: Vec<RobotState>,
    items: Vec<usize>,
    rest: usize,
    clone_num: usize,
    pending_clone: HashSet<Pos>,
    portal_num: usize,
    portals: Vec<Pos>,

    spawn_bot: Option<usize>,

    hist: Vec<Diff>,
    diff: Diff,
    island_size_threshold: i64,
    vects: Vec<Vec<Pos>>,
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
    fn new(
        bd: &Board,
        x: i64,
        y: i64,
        island_size_threshold: i64,
        bought_boosters: &str,
        vects: &[&[Pos]],
        disable_clone: bool,
    ) -> State {
        let mut items = vec![0; 6 + 1];
        for c in bought_boosters.chars() {
            items[booster2u16(char2booster(c)) as usize] += 1;
        }
        let mut pending_clone = HashSet::new();
        let mut clone_num = 0;
        let mut portal_num = 0;
        for y in 0..bd.len() {
            for x in 0..bd[y].len() {
                if bd[y][x].item() == Some(6) {
                    clone_num += 1;
                    pending_clone.insert((x as i64, y as i64));
                }
                if bd[y][x].item() == Some(5) {
                    portal_num += 1;
                }
            }
        }
        let mut has_mystery = false;
        if !disable_clone {
            for y in 0..bd.len() {
                for x in 0..bd[y].len() {
                    if bd[y][x].item() == Some(4) {
                        has_mystery = true;
                        break;
                    }
                }
            }
        }
        if !has_mystery {
            clone_num = 0;
            items[6] = 0;
            pending_clone.clear();
        }

        let mut ret = State {
            bd: bd.clone(),
            robots: vec![],
            items: items,
            rest: 0,
            clone_num,
            pending_clone,
            portal_num,
            portals: vec![],
            spawn_bot: None,

            hist: vec![],
            diff: Diff::default(),
            island_size_threshold: island_size_threshold,
            vects: vects.iter().map(|&r| r.to_owned()).collect(),
        };
        ret.add_robot(x, y);
        ret.rest = ret.paint(x, y, 0, true, false, 0, 1 << 30);
        ret.paint(x, y, 0, false, false, 0, 1 << 30);
        // ret.diff = ret.create_diff();
        ret
    }

    fn add_robot(&mut self, x: i64, y: i64) {
        let mut vect = self.vects[self.robots.len() % self.vects.len()].to_owned();

        self.robots.push(RobotState {
            x,
            y,
            manips: vec![(0, 0), (1, 1), (1, 0), (1, -1)],
            prios: 0,
            num_collected_portal: 0,
            drill_time: 0,
            queue: vec![],
            vect,
            fast_count: 0,
            target: None,
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

            if !self.can_see(x, y, tx, ty) {
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

            for &(dx, dy) in self.robots[i].vect.iter() {
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

                let th = self.island_size_threshold as usize;

                let island_size = self.paint(mx, my, i, true, false, 0, th);
                self.paint(mx, my, i, false, false, 0, th);
                if island_size < self.island_size_threshold as usize {
                    self.paint(mx, my, i, true, true, 0, th);
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

    // fn nearest_empty_dist(&self, i: usize, f: impl Fn(Cell) -> bool) -> usize {
    //     if self.rest == 0 {
    //         0
    //     } else {
    //         nearest_empty_dist(
    //             &self.bd,
    //             self.robots[i].x,
    //             self.robots[i].y,
    //             f,
    //             VECTS[i % VECTS.len()],
    //         )
    //         .unwrap()
    //     }
    // }

    // fn score(&mut self, i: usize) -> f64 {
    //     self.prios as f64 * 100.0 + self.rest as f64 + self.nearest_empty_dist(i) as f64 * 0.01
    // }

    fn overwrap_score(&self, x: i64, y: i64, i: usize) -> i64 {
        if true {
            0
        } else {
            let h = self.bd.len() as i64;
            let w = self.bd[0].len() as i64;

            let mut penalty = 0;

            let manips = self.robots[i].manips.clone();

            let mut bodies = BTreeSet::new();

            for &(dx, dy) in manips.iter() {
                let tx = self.robots[i].x + dx;
                let ty = self.robots[i].y + dy;
                if !(tx >= 0 && tx < w && ty >= 0 && ty < h) {
                    continue;
                }
                bodies.insert((tx, ty));
            }

            for &(dx, dy) in manips.iter() {
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
                    penalty += 3;
                    continue;
                }

                if bodies.contains(&(tx, ty)) {
                    penalty += 1;
                    continue;
                }

                if self.bd[ty as usize][tx as usize].is_painted() {
                    penalty += 3;
                    continue;
                }

                // penalty -= 3;
            }

            penalty
        }
    }

    fn can_see(&self, x1: i64, y1: i64, x2: i64, y2: i64) -> bool {
        // 8近傍以内は絶対見える
        if max((x1 - x2).abs(), (y1 - y2).abs()) <= 1 {
            return !(self.bd[y1 as usize][x1 as usize].is_wall()
                || self.bd[y2 as usize][x2 as usize].is_wall());
        }

        for (px, py) in pass_cells(x1, y1, x2, y2) {
            if self.bd[py as usize][px as usize].is_wall() {
                return false;
            }
        }
        true
    }

    #[allow(dead_code)]
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

fn find_x_closest_bot(state: &State) -> Option<usize> {
    let h = state.bd.len() as i64;
    let w = state.bd[0].len() as i64;
    let bd = &state.bd;

    let mut q = VecDeque::new();
    let mut visited = HashSet::new();
    for y in 0..bd.len() {
        for x in 0..bd[y].len() {
            if bd[y][x].item() == Some(4) {
                q.push_back((x as i64, y as i64));
                visited.insert((x as i64, y as i64));
            }
        }
    }

    let mut goal = HashMap::new();
    for i in 0..state.robots.len() {
        // TODO check if robot is capturing C.
        goal.insert((state.robots[i].x as i64, state.robots[i].y as i64), i);
    }
    if goal.is_empty() {
        return None;
    }
    let goal = goal;
    let vect = &VECTS1[0];

    loop {
        if q.is_empty() {
            return None;
        }
        let (cx, cy) = q.pop_front().unwrap();
        if let Some(&i) = goal.get(&(cx, cy)) {
            return Some(i);
        }

        for &(dx, dy) in vect.iter() {
            let nx = cx + dx;
            let ny = cy + dy;

            if nx < 0 || w <= nx || ny < 0 || h <= ny {
                // Out of map.
                continue;
            }
            if bd[ny as usize][nx as usize].is_wall() {
                continue;
            }
            if visited.contains(&(nx, ny)) {
                continue;
            }
            visited.insert((nx, ny));
            q.push_back((nx, ny));
        }
    }
}

fn collect_item(state: &mut State, i: usize) {
    let x = state.robots[i].x as usize;
    let y = state.robots[i].y as usize;
    if let Some(item) = state.bd[y][x].item() {
        if item != 4 {
            // Not X.
            state.diff.bd.push((x, y, state.bd[y][x]));
            state.items[item] += 1;
            state.bd[y][x].set_item(None);

            // note: clone_numはXがいないと0になってる。
            if item == 6 && state.clone_num > 0 {
                state.clone_num -= 1;
            }

            if item == 5 {
                state.portal_num -= 1;
                state.robots[i].num_collected_portal += 1;
            }
        }
    }
}

fn last_command(sol: &Solution, i: usize) -> Option<Command> {
    if sol.is_empty() {
        return None;
    }
    let lasts = sol.last().unwrap();
    if lasts.len() <= i {
        return None;
    }
    return Some(lasts[i].clone());
}

fn solve(bd_org: &Board, sx: i64, sy: i64, bought_boosters: &str, opt: &SolverOption) -> Solution {
    let h = bd_org.len() as i64;
    let w = bd_org[0].len() as i64;

    let mut state = State::new(
        bd_org,
        sx,
        sy,
        opt.island_size_threshold,
        bought_boosters,
        if opt.change_clone_dir { VECTS1 } else { VECTS2 },
        opt.disable_clone,
    );
    let mut ret: Solution = vec![];
    state.move_to(sx, sy, 0, false);

    // state.dump();

    let mut fin = false;

    let mut steps = 0;
    while !fin {
        let mut cmds = vec![];
        let robot_num = state.robots.len();
        steps += 1;
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
            if state.robots[i].drill_time > 0 {
                state.robots[i].drill_time -= 1;
            }

            // if rand::random::<usize>() % 400 == 0 {
            //     loop {
            //         state.robots[i].vect.shuffle(&mut rand::thread_rng());
            //         if if i % 2 == 0 {
            //             state.robots[i].vect[0].1 == 0 && state.robots[i].vect[1].1 != 0
            //         } else {
            //             state.robots[i].vect[0].1 != 0 && state.robots[i].vect[1].1 == 0
            //         } {
            //             break;
            //         }
            //     }
            // }

            // アイテム取得処理
            collect_item(&mut state, i);

            if state.robots[i].fast_count > 0 {
                state.robots[i].fast_count -= 1;
            }

            if state.robots[i].target == Some((state.robots[i].x, state.robots[i].y)) {
                state.robots[i].target = None;
            }

            // キューに命令が詰まっていたらとりあえずそれをこなす
            if let Some(cmd) = state.robots[i].queue.pop() {
                // FIXME: 他のコマンド使う場合は実装が必要
                match &cmd {
                    Command::Turn(b) => state.rotate(*b, i),
                    Command::Move(dx, dy) => {
                        let nx = state.robots[i].x + dx;
                        let ny = state.robots[i].y + dy;
                        state.move_to(nx, ny, i, true);
                        if state.robots[i].fast_count > 0 {
                            collect_item(&mut state, i);
                            let nnx = nx + dx;
                            let nny = ny + dy;
                            if nnx >= 0
                                && nnx < w
                                && nny >= 0
                                && nny < h
                                && !state.bd[nny as usize][nnx as usize].is_wall()
                            {
                                state.move_to(nnx, nny, i, true);
                            }
                        }
                    }
                    _ => unreachable!(),
                }
                cmds.push(cmd);
                continue;
            }

            if state.items[6] > 0 && state.spawn_bot == None {
                if opt.spawn_delegate {
                    state.spawn_bot = find_x_closest_bot(&state);
                } else {
                    state.spawn_bot = Some(0);
                }
            }

            if Some(i) == state.spawn_bot {
                // TODO: fix ad-hoc ness.
                if let Some((x, y)) = state.robots[i].target {
                    state.pending_clone.insert((x, y));
                    state.robots[i].target = None;
                }
                if state.bd[state.robots[i].y as usize][state.robots[i].x as usize].item()
                    == Some(4)
                {
                    // eprintln!("*****CLONE*****");

                    cmds.push(Command::Clone);
                    state.add_robot(state.robots[i].x, state.robots[i].y);

                    if opt.change_clone_dir {
                        let new_robot_id = state.robots.len() - 1;
                        match new_robot_id % 4 {
                            0 => {}
                            1 => {
                                state.robots[new_robot_id].queue.push(Command::Turn(true));
                            }
                            2 => {
                                state.robots[new_robot_id].queue.push(Command::Turn(true));
                                state.robots[new_robot_id].queue.push(Command::Turn(true));
                            }
                            3 => {
                                state.robots[new_robot_id].queue.push(Command::Turn(false));
                            }
                            _ => unreachable!(),
                        }
                    }

                    state.items[6] -= 1;
                    if state.items[6] == 0 {
                        state.spawn_bot = None;
                    }
                } else {
                    let (nx, ny) = nearest(&state, i, |_, _, c| c.item() == Some(4)).unwrap().0;
                    let dx = nx - state.robots[i].x;
                    let dy = ny - state.robots[i].y;
                    if dx.abs() + dy.abs() == 1 {
                        cmds.push(Command::Move(dx, dy));
                        state.move_to(nx, ny, i, false);
                        if state.robots[i].fast_count > 0 {
                            collect_item(&mut state, i);
                            let nnx = nx + dx;
                            let nny = ny + dy;
                            if !state.bd[nny as usize][nnx as usize].is_wall() {
                                state.move_to(nnx, nny, i, false);
                            }
                        }
                    } else {
                        cmds.push(Command::Teleport(nx, ny));
                        state.move_to(nx, ny, i, false);
                    }
                }
                continue;
            }

            if !state.pending_clone.is_empty() && state.robots[i].target.is_none() && (opt.aggressive_c_collect || i == 0) {
                let (tx, ty) = nearest(&state, i, |x, y, _| state.pending_clone.contains(&(x, y))).unwrap().2;
                state.robots[i].target = Some((tx, ty));
                state.pending_clone.remove(&(tx, ty));
            }

            if !state.robots[i].target.is_none() {
                let (tx, ty) = state.robots[i].target.unwrap();
                let (nx, ny) = nearest(&state, i, |x, y, _| (tx == x && ty == y)).unwrap().0;
                let dx = nx - state.robots[i].x;
                let dy = ny - state.robots[i].y;
                if dx.abs() + dy.abs() == 1 {
                    cmds.push(Command::Move(dx, dy));
                    state.move_to(nx, ny, i, false);
                    if state.robots[i].fast_count > 0 {
                        collect_item(&mut state, i);
                        let nnx = nx + dx;
                        let nny = ny + dy;
                        if 0 <= nnx
                            && nnx < w
                            && 0 <= nny
                            && nny < h
                            && !state.bd[nny as usize][nnx as usize].is_wall()
                        {
                            state.move_to(nnx, nny, i, false);
                        }
                    }
                } else {
                    cmds.push(Command::Teleport(nx, ny));
                    state.move_to(nx, ny, i, false);
                }
                continue;
            }

            if i == 0 {
                /*
                if state.clone_num > 0 {
                    // eprintln!("***** CLONE_NUM: {} *****", state.clone_num);
                    let (nx, ny) = nearest(&state, i, |_, _, c| c.item() == Some(6)).unwrap().0;
                    let dx = nx - state.robots[i].x;
                    let dy = ny - state.robots[i].y;
                    if dx.abs() + dy.abs() == 1 {
                        cmds.push(Command::Move(dx, dy));
                        state.move_to(nx, ny, i, false);
                        if state.robots[i].fast_count > 0 {
                            collect_item(&mut state, i);
                            let nnx = nx + dx;
                            let nny = ny + dy;
                            if 0 <= nnx
                                && nnx < w
                                && 0 <= nny
                                && nny < h
                                && !state.bd[nny as usize][nnx as usize].is_wall()
                            {
                                state.move_to(nnx, nny, i, false);
                            }
                        }
                    } else {
                        cmds.push(Command::Teleport(nx, ny));
                        state.move_to(nx, ny, i, false);
                    }
                    continue;
                }
*/
                if opt.aggressive_teleport
                    && state.portal_num > 0
                    && state.robots[i].num_collected_portal == 0
                {
                    let (nx, ny) = nearest(&state, i, |_, _, c| c.item() == Some(5)).unwrap().0;
                    let dx = nx - state.robots[i].x;
                    let dy = ny - state.robots[i].y;
                    if dx.abs() + dy.abs() == 1 {
                        cmds.push(Command::Move(dx, dy));
                        state.move_to(nx, ny, i, false);
                        if state.robots[i].fast_count > 0 {
                            collect_item(&mut state, i);
                            let nnx = nx + dx;
                            let nny = ny + dy;
                            if 0 <= nnx
                                && nnx < w
                                && 0 <= nny
                                && nny < h
                                && !state.bd[nny as usize][nnx as usize].is_wall()
                            {
                                state.move_to(nnx, nny, i, false);
                            }
                        }
                    } else {
                        cmds.push(Command::Teleport(nx, ny));
                        state.move_to(nx, ny, i, false);
                    }
                    continue;
                }
            }

            // 隣にアイテムがあったら拾う
            if opt.aggressive_item {
                let mut done = false;
                for &(dx, dy) in state.robots[i].vect.iter() {
                    let cx = dx + state.robots[i].x;
                    let cy = dy + state.robots[i].y;
                    if !(cx >= 0 && cx < w && cy >= 0 && cy < h) {
                        continue;
                    }
                    let item = state.bd[cy as usize][cx as usize].item();
                    // 使える奴だけ
                    if item == Some(1)
                        || item == Some(5)
                        || (opt.aggressive_fast && item == Some(2))
                        || (opt.use_drill && item == Some(3))
                    {
                        cmds.push(Command::Move(dx, dy));
                        state.move_to(cx, cy, i, true);
                        if state.robots[i].fast_count > 0 {
                            collect_item(&mut state, i);
                            let nnx = cx + dx;
                            let nny = cy + dy;
                            if 0 <= nnx
                                && nnx < w
                                && 0 <= nny
                                && nny < h
                                && !state.bd[nny as usize][nnx as usize].is_wall()
                            {
                                state.move_to(nnx, nny, i, true);
                            }
                        }
                        done = true;
                        break;
                    }
                }
                if done {
                    // state.dump();
                    continue;
                }
            }

            if state.robots[i].num_collected_portal > 0
                && state.items[5] > 0
                && state.bd[state.robots[i].y as usize][state.robots[i].x as usize].item()
                    != Some(4)
                && !state.bd[state.robots[i].y as usize][state.robots[i].x as usize].has_portal()
                && (!opt.drop_portal_randomly || rand::random::<usize>() % 400 == 0)
            {
                state.robots[i].num_collected_portal -= 1;
                state.portals.push((state.robots[i].x, state.robots[i].y));
                state.bd[state.robots[i].y as usize][state.robots[i].x as usize].set_portal();
                cmds.push(Command::SetPortal);
                continue;
            }

            // 手が増やせるならとりあえず縦に増やす
            if opt.increase_mop {
                if state.items[1] > 0 && i == shortest_mop {
                    state.items[1] -= 1;

                    let dir = *[(0, 1), (1, 0), (0, -1), (-1, 0)]
                        .iter()
                        .filter(|p| state.robots[i].manips.contains(p))
                        .nth(0)
                        .unwrap();

                    for ii in 0.. {
                        let dd = if ii % 2 == 0 { -2 - ii / 2 } else { 2 + ii / 2 };

                        let (dx, dy) = match dir {
                            (0, 1) => (dd, 1),
                            (0, -1) => (dd, -1),
                            (1, 0) => (1, dd),
                            (-1, 0) => (-1, dd),
                            _ => unreachable!(),
                        };

                        if !state.robots[i].manips.contains(&(dx, dy)) {
                            cmds.push(Command::AttachManip(dx, dy));
                            state.robots[i].manips.push((dx, dy));
                            break;
                        }
                    }
                    continue;
                }
            }

            if opt.aggressive_fast && state.robots[i].fast_count == 0 && state.items[2] > 0 {
                cmds.push(Command::AttachWheel);
                state.items[2] -= 1;
                state.robots[i].fast_count = 51; // Containing this turn.
                continue;
            }

            // 塗ってないところに移動する
            // 自分担当の優先島があればそこに行く

            let satisfies = |cc: Cell| -> bool {
                !cc.is_painted()
                    && !cc.is_wall()
                    && if state.robots[i].prios > 0 {
                        cc.prio() == Some(i)
                    } else {
                        true
                    }
            };

            // let n = nearest(&state, i, |x, y, _| {
            //     for &(dmx, dmy) in state.robots[i].manips.iter() {
            //         let mx = x + dmx;
            //         let my = y + dmy;
            //         if mx >= 0
            //             && mx < w
            //             && my >= 0
            //             && my < h
            //             && state.can_see(x, y, mx, my)
            //             && satisfies(state.bd[my as usize][mx as usize])
            //         {
            //             return true;
            //         }
            //     }
            //     false
            // });

            let n = nearest(&state, i, |_, _, cc| satisfies(cc));

            if n.is_none() {
                if i == 0 {
                    return ret;
                }
                fin = true;
                cmds.push(Command::Nop);
                continue;
            }

            let ((nx, ny), dist, _) = n.unwrap();

            if opt.use_drill && dist >= 60 /* TODO: parameterise */ && state.items[3] > 0 {
                let mut found = None;
                'outer: for d in 1..=30_i64 {
                    for j in 0..d {
                        let ps = [(d - j, -j), (-j, -d + j), (-d + j, j), (j, d - j)];
                        for &(dx, dy) in ps.iter() {
                            let xx = state.robots[i].x + dx;
                            let yy = state.robots[i].y + dy;
                            if xx >= 0
                                && xx < w
                                && yy >= 0
                                && yy < h
                                && satisfies(state.bd[yy as usize][xx as usize])
                            {
                                found = Some((xx, yy));
                                break 'outer;
                            }
                        }
                    }
                }

                if let Some((tx, ty)) = found {
                    // dbg!("*** USE DRILL ***");

                    state.items[3] -= 1;
                    cmds.push(Command::StartDrill);
                    state.robots[i].drill_time += 30 + 1;

                    let mut cx = state.robots[i].x;
                    let mut cy = state.robots[i].y;
                    while (cx, cy) != (tx, ty) {
                        if cx != tx {
                            let dx = if cx < tx { 1 } else { -1 };
                            state.robots[i].queue.push(Command::Move(dx, 0));
                            cx += dx;
                        } else {
                            let dy = if cy < ty { 1 } else { -1 };
                            state.robots[i].queue.push(Command::Move(0, dy));
                            cy += dy;
                        }
                    }
                    continue;
                }
            }

            let dx = nx - state.robots[i].x;
            let dy = ny - state.robots[i].y;

            // 移動する方向にモップを回す
            // バグっているので使わないで
            // let mut rot_cnt = 0;
            // while !state.robots[i].manips.contains(&(dx, dy)) {
            //     state.robots[i].rotate(true);
            //     rot_cnt += 1;
            // }

            // if rot_cnt == 0 {
            // } else if rot_cnt == 1 {
            //     cmds.push(Command::Turn(true));
            // } else if rot_cnt == 2 {
            //     cmds.push(Command::Turn(true));
            //     cmds.push(Command::Turn(true));
            // } else {
            //     cmds.push(Command::Turn(false));
            // }

            // dbg!((rot_cnt, &state.manips));

            // 移動
            if dx.abs() + dy.abs() == 1 {
                // If the direction was as same as last one, and it's not the
                // direction of the mop, turn.
                if opt.mop_direction
                    && !state.robots[i].manips.contains(&(dx, dy))
                    && last_command(&ret, i) == Some(Command::Move(dx, dy))
                {
                    if state.robots[i].manips.contains(&(-dx, -dy)) {
                        cmds.push(Command::Turn(true));
                        state.rotate(true, i);
                        state.robots[i].queue.push(Command::Turn(true));
                    } else if !state.robots[i].manips.contains(&(dy, -dx)) {
                        cmds.push(Command::Turn(true));
                        state.rotate(true, i);
                    } else {
                        cmds.push(Command::Turn(false));
                        state.rotate(false, i);
                    }
                    state.robots[i].queue.push(Command::Move(dx, dy));
                } else {
                    cmds.push(Command::Move(dx, dy));
                    state.move_to(nx, ny, i, true);
                    if state.robots[i].fast_count > 0 {
                        collect_item(&mut state, i);
                        let nnx = nx + dx;
                        let nny = ny + dy;
                        if 0 <= nnx
                            && nnx < w
                            && 0 <= nny
                            && nny < h
                            && !state.bd[nny as usize][nnx as usize].is_wall()
                        {
                            state.move_to(nnx, nny, i, true);
                        }
                    }
                }
            } else {
                cmds.push(Command::Teleport(nx, ny));
                state.move_to(nx, ny, i, true);
            }
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
    show_solution: bool,
    force_save: bool,
    bought_boosters: &str,
    solver_option: &SolverOption,
) -> Result<()> {
    let mut input = input.clone();
    let (w, h) = normalize(&mut input);
    let mut bd = build_map(&input, w, h);

    let ans = solve(
        &mut bd,
        input.init_pos.0,
        input.init_pos.1,
        bought_boosters,
        solver_option,
    );

    let score = ans.len() as i64;
    eprintln!("Score: {}, (options: {:?})", score, solver_option);

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
        save_solution(LIGHTNING_DIR, name, &ans, score, force_save)?;
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

fn save_solution(
    root: &str,
    name: &str,
    ans: &Solution,
    score: i64,
    force_save: bool,
) -> Result<()> {
    // println!("*****: {:?} {:?} {:?} {}", root, name, score);

    let best = get_best_score(root, name)?.unwrap_or(0);
    if best == 0 || best > score || force_save {
        if best == 0 || best > score {
            eprintln!("* Best score for {}: {} -> {}", name, best, score);
        }
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

fn main() {
    let builder = std::thread::Builder::new();
    let th = builder.stack_size(16 * (1 << 20));

    let handle = th.spawn(|| main_process().unwrap()).unwrap();
    let _ = handle.join();
}

fn main_process() -> Result<()> {
    match Opt::from_args() {
        Opt::Solve {
            show_solution,
            force_save,
            input,
            bought_boosters,
            solver_option,
        } => {
            let (con, file) = read_file(&input)?;
            let problem = parse_input(&con)?;
            solve_lightning(
                &get_problem_name(&file),
                &problem,
                show_solution,
                force_save,
                &bought_boosters,
                &solver_option,
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
