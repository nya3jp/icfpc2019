use chrono::prelude::*;
use regex::Regex;
use std::cmp::{max, min};
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::env;
use std::fs::{self, File};
use std::io::prelude::*;
use std::path::{Path, PathBuf};
use structopt::StructOpt;

type Result<T> = std::result::Result<T, Box<std::error::Error>>;
type Board = Vec<Vec<u8>>;

#[derive(Debug, StructOpt)]
enum Opt {
    #[structopt(name = "solve")]
    Solve { input: PathBuf },

    #[structopt(name = "pack")]
    Pack,
}

type Pos = (i64, i64);

const VECT: &[Pos] = &[(1, 0), (-1, 0), (0, 1), (0, -1)];

#[derive(Debug, Clone)]
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
            let c = if bd[y][x] & (1 << 4) != 0 {
                'B'
            } else if bd[y][x] & (1 << 5) != 0 {
                'F'
            } else if bd[y][x] & (1 << 6) != 0 {
                'L'
            } else if bd[y][x] & (1 << 7) != 0 {
                'X'
            } else if bd[y][x] & (1 << 1) != 0 {
                '.'
            } else if bd[y][x] & 1 == 0 {
                ' '
            } else {
                '#'
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
            Booster::F => bd[*y as usize][*x as usize] |= 1 << 5,
            Booster::L => bd[*y as usize][*x as usize] |= 1 << 6,
            Booster::X => bd[*y as usize][*x as usize] |= 1 << 7,
        }
    }

    bd
}

// solution

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
}

fn solve(bd: &mut Vec<Vec<u8>>, sx: i64, sy: i64) -> Vec<Command> {
    let h = bd.len() as i64;
    let w = bd[0].len() as i64;

    let mut cx = sx;
    let mut cy = sy;
    let mut items = vec![0; 4];
    let mut manips = vec![(0, 0), (1, 1), (1, 0), (1, -1)];

    for &(dx, dy) in manips.iter() {
        let tx = cx + dx;
        let ty = cy + dy;
        if !(tx >= 0 && tx < w && ty >= 0 && ty < h) {
            continue;
        }
        if bd[ty as usize][tx as usize] & 1 != 0 {
            continue;
        }
        bd[ty as usize][tx as usize] |= 2;
    }
    // bd[cy as usize][cx as usize] |= 2;

    let mut ret = vec![];

    while let Some((nx, ny)) = nearest(&bd, cx, cy) {
        // dbg!((nx, ny));
        ret.push(Command::Move(nx - cx, ny - cy));
        cx = nx;
        cy = ny;

        for &(dx, dy) in manips.iter() {
            let tx = cx + dx;
            let ty = cy + dy;
            if !(tx >= 0 && tx < w && ty >= 0 && ty < h) {
                continue;
            }
            if bd[ty as usize][tx as usize] & 1 != 0 {
                continue;
            }
            bd[ty as usize][tx as usize] |= 2;
        }
        // bd[cy as usize][cx as usize] |= 2;
        // print_bd(&bd);
        // print_ans(&ret);
    }

    ret
}

//-----

fn solve_lightning(name: &str, input: &Input) -> Result<()> {
    let mut input = input.clone();
    let (w, h) = normalize(&mut input);
    let mut bd = build_map(&input, w, h);
    let ans = solve(&mut bd, input.init_pos.0, input.init_pos.1);
    // print_ans(&ans);

    let score = ans.len() as i64;
    // eprintln!("Score: {}", score);

    let bs = get_best_score(LIGHTNING_DIR, name).unwrap();

    println!("Score for {}: score = {}", name, score);

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
        Opt::Solve { input } => {
            let problem = read_input(&input)?;
            solve_lightning(&get_problem_name(&input), &problem)?;
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
