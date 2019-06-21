use structopt::StructOpt;
use std::io::prelude::*;
use std::path::{Path, PathBuf};
use std::fs::File;
use std::cmp::{min, max};
use std::collections::{BTreeMap, BTreeSet, VecDeque};

type Result<T> = std::result::Result<T, Box<std::error::Error>>;
type Board = Vec<Vec<u8>>;

#[derive(Debug, StructOpt)]
struct Opt {
    input: PathBuf,
}

type Pos = (i64, i64);

const VECT: &[Pos] = &[
    (0, 1),
    (0, -1),
    (1, 0),
    (-1, 0),
];

#[derive(Debug)]
enum Booster {
    B,
    F,
    L,
    X,
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

#[derive(Debug)]
struct Input {
    map: Vec<Pos>,
    init_pos: Pos,
    obstacles: Vec<Vec<Pos>>,
    boosters: Vec<(Booster, Pos)>,
}

fn parse_pos(s: &str) -> Result<Pos> {
    // dbg!(s);
    let s = s.chars().map(|c| if c.is_ascii_digit() {c} else {' '}).collect::<String>();
    let mut it = s.split_whitespace().map(|w| w.parse().unwrap());
    Ok((it.next().unwrap(), it.next().unwrap()))
}

fn parse_pos_list(s: &str) -> Result<Vec<Pos>> {
    // dbg!(s);
    s.split("),(").filter(|w| w.trim() != "").map(parse_pos).collect()
}

fn parse_pos_list_list(s: &str) -> Result<Vec<Vec<Pos>>> {
    // dbg!(s);
    s.split(';').filter(|w| w.trim() != "").map(parse_pos_list).collect()
}

fn parse_booster(s: &str) -> Result<(Booster, Pos)> {
    let ty = match &s[0..1] {
        "B" => Booster::B,
        "F" => Booster::F,
        "L" => Booster::L,
        "X" => Booster::X,
        _ => unreachable!(),
    };

    let pos = parse_pos(&s[1..])?;

    Ok((ty, pos))
}

fn parse_booster_list(s: &str) -> Result<Vec<(Booster, Pos)>> {
    s.split(';').filter(|w| w.trim() != "").map(parse_booster).collect()
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
            let c =
                if bd[y][x] & (1<<4) != 0 {'B'} else
                if bd[y][x] & (1<<5) != 0 {'F'} else
                if bd[y][x] & (1<<6) != 0 {'L'} else
                if bd[y][x] & (1<<7) != 0 {'X'} else
                if bd[y][x] & (1<<1) != 0 {'.'} else
                if bd[y][x] & 1 == 0 {' '} else
                {'#'};
            eprint!("{}", c);
        }
        eprintln!();
    }
    eprintln!();
}

fn print_ans(ans: &[Command]) {
    for cmd in ans.iter() {
        match cmd {
            Command::Move(0, 1) => print!("W"),
            Command::Move(0, -1) => print!("S"),
            Command::Move(-1, 0) => print!("A"),
            Command::Move(1, 0) => print!("D"),
            Command::Move(_, _) => panic!("Invalid move: {:?}", cmd),

            Command::Nop => print!("Z"),
            Command::Turn(true) => print!("E"),
            Command::Turn(false) => print!("Q"),
            Command::AttachManip(dx, dy) => print!("B({},{})", dx, dy),
            Command::AttachWheel => print!("F"),
            Command::StartDrill => print!("L"),
        }
    }
    println!();
}

fn build_map(input: &Input, w: i64, h: i64) -> Vec<Vec<u8>> {
    let mut bd = vec![vec![1; w as usize]; h as usize];
    for y in 0..h {
        let mut ss = BTreeSet::new();
        for i in 0..input.map.len() {
            let (x1, y1) = input.map[i];
            let (x2, y2) = input.map[(i+1)%input.map.len()];

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
            for j in ss[i*2]..ss[i*2+1] {
                bd[y as usize][j as usize] = 0;
            }
        }
    }

    for y in 0..h {
        let mut ss = BTreeSet::new();
        for j in 0..input.obstacles.len() {
            for i in 0..input.obstacles[j].len() {
                let (x1, y1) = input.obstacles[j][i];
                let (x2, y2) = input.obstacles[j][(i+1)%input.obstacles[j].len()];

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
            for j in ss[i*2]..ss[i*2+1] {
                bd[y as usize][j as usize] = 1;
            }
        }
    }

    for (typ, (x, y)) in input.boosters.iter() {
        match typ {
            Booster::B => bd[*y as usize][*x as usize] |= 1 << 4,
            Booster::F => bd[*y as usize][*x as usize] |= 1 << 5,
            Booster::L => bd[*y as usize][*x as usize] |= 1 << 6,
            Booster::X => bd[*y as usize][*x as usize] |= 1 << 7,
        }
    }

    bd
}

fn nearest(bd: &Vec<Vec<u8>>, x: i64, y: i64) -> Option<(i64, i64)> {
    let h = bd.len() as i64;
    let w = bd[0].len() as i64;

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

            if bd[ny as usize][nx as usize] & 2 == 0 {
                found = Some((nx, ny));
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

    loop {
        let n = prev[&(tx, ty)];
        if n == (x, y) {
            return Some((tx, ty));
        }
        tx = n.0;
        ty = n.1;
    }
}

fn solve(bd: &mut Vec<Vec<u8>>, sx: i64, sy: i64) -> Vec<Command>{
    let w = bd.len() as i64;
    let h = bd[0].len() as i64;

    let mut cx = sx;
    let mut cy = sy;
    let mut items = vec![0; 4];
    let mut manips = vec![/*(1, 1), (1, 0), (1, -1), */(0, 0)];

    // for &(dx, dy) in manips.iter() {
    //     let tx = cx + dx;
    //     let ty = cy + dy;
    //     if !(tx >= 0 && tx < w && ty >= 0 && ty < h) {
    //         continue;
    //     }
    //     bd[ty as usize][tx as usize] |= 2;
    // }
    bd[cy as usize][cx as usize] |= 2;

    let mut ret = vec![];

    while let Some((nx, ny)) = nearest(&bd, cx, cy) {
        // dbg!((nx, ny));
        ret.push(Command::Move(nx-cx, ny-cy));
        cx = nx;
        cy = ny;
        // for &(dx, dy) in manips.iter() {
        //     let tx = cx + dx;
        //     let ty = cy + dy;
        //     if !(tx >= 0 && tx < w && ty >= 0 && ty < h) {
        //         continue;
        //     }
        //     bd[ty as usize][tx as usize] |= 2;
        // }

        bd[cy as usize][cx as usize] |= 2;

        // print_bd(&bd);

        // print_ans(&ret);
    }

    ret
}

fn main() -> Result<()> {
    let opt = Opt::from_args();

    let mut input = read_input(&opt.input)?;
    dbg!(input.init_pos);
    let (w, h) = normalize(&mut input);
    dbg!(input.init_pos);

    let mut bd = build_map(&input, w, h);
    print_bd(&bd);

    let ans = solve(&mut bd, input.init_pos.0, input.init_pos.1);
    // dbg!(&ans);
    print_ans(&ans);

    Ok(())
}
