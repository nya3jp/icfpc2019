use super::{Result, Pos, parse_pos_list};
use rand::seq::SliceRandom;
use rand::thread_rng;

#[derive(Debug)]
pub struct PuzzleInput {
    b_num: usize,
    e_num: usize,
    t_size: usize,
    v_min: usize,
    v_max: usize,
    m_num: usize,
    f_num: usize,
    d_num: usize,
    r_num: usize,
    c_num: usize,
    x_num: usize,
    i_sqs: Vec<Pos>,
    o_sqs: Vec<Pos>,
}

pub fn parse_puzzle(s: &str) -> Result<PuzzleInput> {
    let mut jt = s.split('#');

    let mut it = jt.next().unwrap().split(',');

    let b_num = it.next().unwrap().parse().unwrap();
    let e_num = it.next().unwrap().parse().unwrap();
    let t_size = it.next().unwrap().parse().unwrap();
    let v_min = it.next().unwrap().parse().unwrap();
    let v_max = it.next().unwrap().parse().unwrap();
    let m_num = it.next().unwrap().parse().unwrap();
    let f_num = it.next().unwrap().parse().unwrap();
    let d_num = it.next().unwrap().parse().unwrap();
    let r_num = it.next().unwrap().parse().unwrap();
    let c_num = it.next().unwrap().parse().unwrap();
    let x_num = it.next().unwrap().parse().unwrap();

    let i_sqs = parse_pos_list(jt.next().unwrap())?;
    let o_sqs = parse_pos_list(jt.next().unwrap())?;

    Ok(PuzzleInput {
        b_num,
        e_num,
        t_size,
        v_min,
        v_max,
        m_num,
        f_num,
        d_num,
        r_num,
        c_num,
        x_num,
        i_sqs,
        o_sqs,
    })
}

pub fn solve_puzzle(input: &PuzzleInput) {
    let n = input.t_size;
    let mut bd = vec![vec!['#'; n]; n];

    for y in 1..n - 1 {
        for x in 1..n - 1 {
            bd[y][x] = ' ';
        }
    }

    for &(x, y) in input.i_sqs.iter() {
        bd[y as usize][x as usize] = 'o';
    }

    bd[input.o_sqs[0].1 as usize][input.o_sqs[0].0 as usize] = '#';

    for &(x, y) in input.o_sqs[1..].iter() {
        let b = rec_gen(&mut bd, x, y);
        // assert!(b);

        paint_puzzle(&mut bd, x, y);

        for row in bd.iter() {
            eprintln!("{}", row.iter().collect::<String>());
        }
        eprintln!("===");
    }
}

fn rec_gen(bd: &mut Vec<Vec<char>>, x: i64, y: i64) -> bool {
    // for row in bd.iter() {
    //     eprintln!("{}", row.iter().collect::<String>());
    // }
    // eprintln!("===");

    let n = bd.len() as i64;
    if !(x >= 0 && x < n && y >= 0 && y < n) {
        return true;
    }

    if bd[y as usize][x as usize] == '#' {
        return true;
    }

    if bd[y as usize][x as usize] == 'o' {
        return false;
    }

    bd[y as usize][x as usize] = '*';

    let mut ord = [0, 1, 2, 3];

    let mut rng = thread_rng();
    ord.shuffle(&mut rng);

    let vect = &[(0, 1), (1, 0), (0, -1), (-1, 0)];

    for &o in ord.iter() {
        let (dx, dy) = vect[o];

        let nx = x + dx;
        let ny = y + dy;

        let mut ok = true;
        for &(ex, ey) in vect.iter() {
            let mx = nx + ex;
            let my = ny + ey;

            if (x, y) == (mx, my) {
                continue;
            }

            if mx >= 0 && mx < n && my >= 0 && my < n && bd[my as usize][mx as usize] == '*' {
                ok = false;
                break;
            }
        }
        if !ok {
            continue;
        }

        if rec_gen(bd, x + dx, y + dy) {
            return true;
        }
    }

    bd[y as usize][x as usize] = ' ';
    return false;
}

fn paint_puzzle(bd: &mut Vec<Vec<char>>, x: i64, y: i64) {
    let h = bd.len() as i64;
    let w = bd[0].len() as i64;

    if !(x >= 0 && x < w && y >= 0 && y < h) {
        return;
    }

    if bd[y as usize][x as usize] != '*' {
        return;
    }

    bd[y as usize][x as usize] = '#';
    paint_puzzle(bd, x + 1, y);
    paint_puzzle(bd, x - 1, y);
    paint_puzzle(bd, x, y + 1);
    paint_puzzle(bd, x, y - 1);
}
