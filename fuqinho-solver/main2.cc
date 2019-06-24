#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <algorithm>
#include <queue>
#include <iostream>
#include <sstream>
#include <vector>
#include <queue>
#include <stack>
#include <set>
#include <map>
#include <bitset>
#include <algorithm>
#include <numeric>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cassert>
#include <random>

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "fuqinho-solver/mine.h"

using namespace std;

#define REP(i,n) for(int i=0; i<(int)(n); i++)
#define FOR(i,b,e) for(int i=(b); i<(int)(e); i++)
#define EACH(i,c) for(__typeof((c).begin()) i=(c).begin(); i!=(c).end(); i++)
#define ALL(c) (c).begin(), (c).end()
#define dump(x) cerr << #x << " = " << (x) << endl;
typedef long long ll;

template<class T1,class T2> ostream& operator<<(ostream& o,const pair<T1,T2>& p){return o<<"("<<p.first<<","<<p.second<<")";}
template<class T> ostream& operator<<(ostream& o,const vector<T>& v){o<<"[";for(typename vector<T>::const_iterator i=v.begin();i!=v.end();++i){if (i != v.begin()) o << ", ";o<<(*i);}o<<"]";return o;}
template<class T> ostream& operator<<(ostream& o,const set<T>& s){o<<"{";for(typename set<T>::const_iterator i=s.begin();i!=s.end();++i){if(i!=s.begin())o<<", ";o<<(*i);}o<<"}";return o;}
template<class K,class V> ostream& operator<<(ostream& o,const map<K,V>& m){o<<"{";for(typename map<K,V>::const_iterator i=m.begin();i!=m.end();++i){if(i!=m.begin())o<<", ";o<<i->first<<":"<<i->second;}o<<"}";return o;}
template<class T> ostream& operator<<(ostream& o,const vector<vector<T> >& m){o<<"[\n";for(typename vector<vector<T> >::const_iterator i=m.begin();i!=m.end();++i){o<<"  "<<(*i);o<<(i+1!=m.end()?",\n":"\n");}o<<"]\n";return o;}

string bitstr(int n,int d=0){string r;for(int i=0;i<d||n>0;++i,n>>=1){r+=(n&1)?"1":"0";}reverse(r.begin(),r.end());return r;}

constexpr int INF = 1e9;
const char MOVE_TYPES[] = "WSADEQBF";
const int DX[] = {1, 0, -1, 0};
const int DY[] = {0, 1, 0, -1};
const char DXY_DIR[] = "DWAS";

struct Move {
    char move_type;
    int dx;
    int dy;
    Move() {}
    Move(char type) : move_type(type) {}
};

bool operator< (const Move& lhs, const Move& rhs) {
    return lhs.move_type < rhs.move_type;
}

// ========= Flow ==========
// Choose move-0 for robot-0
// Choose move-1 for robot-1
// Apply move-0 to robot-0
// Apply move-1 to robot-1
// Paint the floor
// Calculate the score.
// Remember move-0 and move-1 with the score

// ========= Flow 2 =========
// Choose move-0 for robot-0
// Calculate the score.
// Choose move-1 for robot-1
// 

// Robot's basic behavior
// 1. Determine the area to paint or booster to take. (BFS)
// 2. If it is far, take the shortest path to the area.
// 3. If it is near (less than 3 cells), start beam search to determine how to paint.
// 4. Once all the target area were painted, or the target area was devided, recalculate the area.

// =========== OR ===========
// Do the following every turn.
// Determine the area to paint for each 

// Boosters:
//   B: Use immediately.
//   F: Use when the target area is far, or the move has high score in the beam search.
//   R: Use immediately. T: Consider when calculate the shortest path to the target area.
//   C: Take and use it as soon as possible.
//   L: Use immediately.

// Robot mode:
//   G: Go to a position P (e.g. Take C booster immediately)
//   A: Approach to a target area (Want to paint, but it's far)
//   P: Paint the area nearby using beam search.

// Conductor behavior:
//   1. If C > 0, set mode G to a robot which is near X cell.
//   2. If C == 0 and there are C cell, set mode G to a robot which is near C cell.
//   3. For free robots (not in mode G), assign cells to each robots and set up target area.
//      If multiple robots in the same place, assume that one is at neighbor cell.
//      First, calculate the cover area for each robot based on the distance from the robot.
//      Second, inside the cover area, find the empty area around a robot (up to 3 cell) and fill the area.
//          If no empty area is found, set mode A and approach to the area.
//          If there are multiple disconnected areas, set the smallest area as taget area and set mode to P.
//          If there is a single empty area, set it as target area and set mode to P.
//   4. Move each robots following their mode.

struct Robot {
    int x, y;
    int t0;
    vector<tuple<int, int>> arms;
    int tB, tF;

    Robot(int x, int y) : x(x), y(y), tB(0), tF(0) {
        arms.push_back(make_tuple(0, 0));
        arms.push_back(make_tuple(1, 0));
        arms.push_back(make_tuple(1, -1));
        arms.push_back(make_tuple(1, 1));
    }
};

struct RobotOrder {
    // W: Waiting an order
    // G: Follow target_path
    // P: Paint target area (B, F, L, T are allowed.)
    // C: Use C booster by following |target_path| to X;
    // R: Use R at the current pos;
    char mode;
    vector<vector<bool>> target_area;
    vector<tuple<int, int>> target_path;
    int turn_to_expire;
    RobotOrder() : mode('W') {}
    void print() {
        cerr << "Mode: " << mode << endl;
        if (!target_area.empty()) {
            for (int i = target_area.size()-1; i>=0; i--) {
                REP(j, target_area[0].size())
                    cerr << (target_area[i][j] ? 'T' : ' ');
                cerr << endl;
            }
        }
    }
};

struct State {
    std::vector<std::string> map;
    std::vector<std::vector<bool>> painted;
    vector<Robot> robots;

    int B, F, L, R, C;
    int rB, rF, rL, rR, rC;
    int gotB, gotF, gotL, gotR, gotC;

    // Computed states
    int num_unpainted;
    vector<vector<int>> cell_scores;

    State(const Mine& mine, const string boosters) {
        int W = mine.max_x, H = mine.max_y;
        robots.push_back(Robot(mine.cur_x, mine.cur_y));
        B = F = L = R = C = 0;
        rB = rF = rL = rR = rC = 0;
        gotB = gotF = gotL = gotR = gotC = 0;
        for (char c : boosters) {
            if (c == 'B') B++;
            if (c == 'F') F++;
            if (c == 'L') L++;
            if (c == 'R') R++;
            if (c == 'C') C++;
        }
        
        map = std::vector<std::string>(H, std::string(W, '.'));
        painted = std::vector<std::vector<bool>>(H, std::vector<bool>(W, false));

        // Computed states
        for (size_t i = 0; i < mine.mp.size(); i++) {
            for (size_t j = 0; j < mine.mp[0].size(); j++) {
                if (mine.mp[i][j] == 0) { // Empty
                    map[i][j] = '.';
                    painted[i][j] = false;
                } else if (mine.mp[i][j] == 1) { // Obstacle
                    map[i][j] = '#';
                    painted[i][j] = true;
                } else if (mine.mp[i][j] == 2) {  // Wrapped.
                }
            }
        }
        parseBoosters(mine.boosters);
        paint();
    }

    void enableReservedBoosters() {
        B += rB;
        F += rF;
        L += rL;
        R += rR;
        C += rC;
        rB = rF = rL = rR = rC = 0;
    }

    void parseBoosters(std::string str) {
        if (str.empty())
            return;

        int num_boosters = count(str.begin(), str.end(), ';') + 1;
        replace(str.begin(), str.end(), ';', ' ');
        replace(str.begin(), str.end(), '(', ' ');
        replace(str.begin(), str.end(), ')', ' ');
        replace(str.begin(), str.end(), ',', ' ');
        stringstream ss(str);
        for (int i=0; i<num_boosters; i++) {
            char type;
            int x, y;
            ss >> type >> x >> y;
            map[y][x] = type;
        }
    }

    void paint() {
        for (auto& robot : robots) {
            for (auto arm : robot.arms) {
                int x = robot.x + get<0>(arm);
                int y = robot.y + get<1>(arm);
                if (x >= 0 && x < (int)map[0].size() && y >= 0 && y < (int)map.size()) {
                    painted[y][x] = true;
                }
            }
        }
        updateComputedStates();
    }

    void updateComputedStates() {
        num_unpainted = 0;
        for (int i=0; i<(int)painted.size(); i++) {
            for (int j=0; j<(int)painted[i].size(); j++) {
                if (!painted[i][j]) num_unpainted++;
            }
        }
    }

    void computeCellScores() {
        cell_scores = vector<vector<int>>(painted.size(), vector<int>(painted[0].size(), 0));
        REP(i, cell_scores.size()) {
            REP(j, cell_scores[i].size()) {
                if (map[i][j] == '#' || painted[i][j])
                    continue;
                int adjacent_nontarget = 0;
                for (int k=0; k<4; k++) {
                    int nr = i + DY[k];
                    int nc = j + DX[k];
                    if (nr >= 0 && nr < (int)cell_scores.size() && nc >= 0 && nc < (int)cell_scores[0].size()) {
                        if (map[nr][nc] == '#' || painted[nr][nc])
                            adjacent_nontarget++;
                    }
                }
                cell_scores[i][j] = (adjacent_nontarget + 1) * (adjacent_nontarget + 1);
                // cell_scores[i][j] = 1;
            }
        }
    }

    double score() const {
        // Consider cells already painted.
        double score = 0.0;
        for (int i = 0; i < (int)painted.size(); i++) {
            for (int j = 0; j < (int)painted[i].size(); j++) {
                if (painted[i][j]) {
                    score += 100 * cell_scores[i][j];
                }
            }
        }
        // Consider distance to nearlest dot;
        const int R = (int)robots.size();
        REP(k, R) {
            int dist_to_nearest = INF;
            queue<tuple<int, int, int>> que;
            vector<vector<bool>> visited(map.size(), vector<bool>(map[0].size(), false));
            visited[robots[k].y][robots[k].x] = true;
            que.push(make_tuple(0, robots[k].x, robots[k].y));
            bool found = false;
            while (!que.empty() && !found) {
                int d = get<0>(que.front());
                int x = get<1>(que.front());
                int y = get<2>(que.front());
                que.pop();
                for (int k=0; k<4; k++) {
                    int nd = d+1;
                    int nx = x + DX[k];
                    int ny = y + DY[k];
                    if (nx < 0 || nx >= (int)map[0].size() || ny < 0 || ny >= (int)map.size())
                        continue;
                    if (visited[ny][nx])
                        continue;
                    if (map[ny][nx] == '#')
                        continue;

                    if (!painted[ny][nx]) {
                        found = true;
                        dist_to_nearest = nd;
                        break;
                    }
                    visited[ny][nx] = true;
                    que.push(make_tuple(nd, nx, ny));
                }
            }
            if (dist_to_nearest != INF)
                score -= dist_to_nearest;
        }

        // Consider boosters
        score += gotF * 10000;
        score += gotB * 10000;

        return score;
    }

    bool can(int r_idx, char move_type) const {
        if (move_type == 'W' || move_type == 'S' || move_type == 'A' || move_type == 'D') {
            int dir_index = 0;
            if (move_type == 'W') dir_index = 1;
            if (move_type == 'A') dir_index = 2;
            if (move_type == 'S') dir_index = 3;
            int next_x = robots[r_idx].x + DX[dir_index];
            int next_y = robots[r_idx].y + DY[dir_index];
            return next_x >= 0 && next_x < (int)map[0].size() &&
                next_y >= 0 && next_y < (int)map.size() &&
                map[next_y][next_x] != '#';
        } else if (move_type == 'E' || move_type == 'Q') {
            return true;
        } else if (move_type == 'B') {
            return B > 0;
        } else if (move_type == 'F') {
            return F > 0;
        } else if (move_type == 'C') {
            cerr << "------- check if C is doable ---" << endl;
            cerr << "robot = (" << robots[r_idx].x << "," << robots[r_idx].y << ")" << endl;
            cerr << "map(x, y) = " << map[robots[r_idx].y][robots[r_idx].x] << endl;;
            return C > 0 && map[robots[r_idx].y][robots[r_idx].x] == 'X';
        } else if (move_type == 'Z') {
            return true;
        } else {
            // TODO: support other operations.
            return false;
        }
       return false;
    }

    pair<Move, State> move(int r_idx, char move_type) const {
        State next_state = *this;
        if (!can(r_idx, move_type)) {
            return make_pair(Move('Z'), next_state);
        }
        int b_dx = 0;
        int b_dy = 0;
        if (move_type == 'W' || move_type == 'S' || move_type == 'A' || move_type == 'D') {
            int dir_index = 0;
            if (move_type == 'W') dir_index = 1;
            if (move_type == 'A') dir_index = 2;
            if (move_type == 'S') dir_index = 3;
            int turns = ((robots[r_idx].tF > 0) ? 2 : 1);
            for (int t=0; t<turns; t++) {
                int next_x = next_state.robots[r_idx].x + DX[dir_index];
                int next_y = next_state.robots[r_idx].y + DY[dir_index];
                if (next_x < 0 || next_x >= (int)next_state.map[0].size() ||
                        next_y < 0 || next_y >= (int)next_state.map.size() ||
                        next_state.map[next_y][next_x] == '#')
                    continue;
                next_state.robots[r_idx].x = next_x;
                next_state.robots[r_idx].y = next_y;
                if (next_state.map[next_y][next_x] == 'B') {
                    next_state.rB++;
                    next_state.gotB++;
                    next_state.map[next_y][next_x] = '.';
                }
                if (map[next_y][next_x] == 'F') {
                    next_state.rF++;
                    next_state.gotF++;
                    next_state.map[next_y][next_x] = '.';
                }
                if (map[next_y][next_x] == 'L') {
                    next_state.rL++;
                    next_state.gotL++;
                    next_state.map[next_y][next_x] = '.';
                } 
                if (map[next_y][next_x] == 'R') {
                    next_state.rR++;
                    next_state.gotR++;
                    next_state.map[next_y][next_x] = '.';
                } 
                if (map[next_y][next_x] == 'C') {
                    next_state.rC++;
                    next_state.gotC++;
                    next_state.map[next_y][next_x] = '.';
                } 
                next_state.paint();
            }
        } else if (move_type == 'E') { // clockwize
            for (size_t i = 0; i < robots[r_idx].arms.size(); i++) {
                int dx = get<0>(robots[r_idx].arms[i]);
                int dy = get<1>(robots[r_idx].arms[i]);
                next_state.robots[r_idx].arms[i] = make_tuple(dy, -dx);
            }
            next_state.paint();
        } else if (move_type == 'Q') { // counter-clockwize
            for (size_t i = 0; i < robots[r_idx].arms.size(); i++) {
                int dx = get<0>(robots[r_idx].arms[i]);
                int dy = get<1>(robots[r_idx].arms[i]);
                next_state.robots[r_idx].arms[i] = make_tuple(-dy, dx);
            }
            next_state.paint();
        } else if (move_type == 'F') {
            if (next_state.robots[r_idx].tF > 0)
                next_state.robots[r_idx].tF += 50;
            else
                next_state.robots[r_idx].tF = 51;
            next_state.F = F - 1;

        } else if (move_type == 'B') {
            const int BX[] = {-1, -1, -1, 0, 0};
            const int BY[] = {0, 1, -1, 1, -1};
            int k = robots[r_idx].arms.size() - 4;
            if (k < 5) {
                int bx = BX[k];
                int by = BY[k];
                if (robots[r_idx].arms[1] == make_tuple(1, 0)) {
                } else if (robots[r_idx].arms[1] == make_tuple(0, 1)) {
                    bx = -BY[k];
                    by = BX[k];
                } else if (robots[r_idx].arms[1] == make_tuple(-1, 0)) {
                    bx = -BX[k];
                    by = -BY[k];
                } else {
                    bx = BY[k];
                    by = -BX[k];
                }
                b_dx = bx;
                b_dy = by;
                next_state.robots[r_idx].arms.push_back(make_tuple(bx, by));
            }
            next_state.B = B - 1;
        } else if (move_type == 'C') {
            next_state.C--;
            next_state.robots.push_back(Robot(robots[r_idx].x, robots[r_idx].y));
        }

        next_state.robots[r_idx].tF = max(0, next_state.robots[r_idx].tF-1);
        Move this_move;
        this_move.move_type = move_type;
        this_move.dx = b_dx;
        this_move.dy = b_dy;
        return make_pair(this_move, next_state);
    }

    bool finished() const {
        return num_unpainted == 0;
    }

    void print() {
        print(vector<RobotOrder>());
    }

    void print(const vector<RobotOrder>& orders) {
        const int H = (int)map.size();
        const int W = (int)map[0].size();

        cerr << "===============================" << endl;
        cerr << "B:" << B << " F:" << F << " C:" << C << " L:" << L << " R:" << R << endl;
        REP(i, robots.size()) {
            cerr << "R" << i << " ";
            if ((int)orders.size() > i) {
                cerr << " mode:" << orders[i].mode << " ";
            }
            cerr << " (" << robots[i].x << "," << robots[i].y << ") "
            << "tB:" << robots[i].tB << " tF:" << robots[i].tF << endl;
        }
        vector<string> display = map;
        
        REP (i, display.size()) REP(j, display[i].size()) {
            if (display[i][j] == '.' && painted[i][j]) {
                display[i][j] = ' ';
            }
        }
        REP (k, robots.size()) {
            REP(i, robots[k].arms.size()) {
                int x = robots[k].x + get<0>(robots[k].arms[i]);
                int y = robots[k].y + get<1>(robots[k].arms[i]);
                if (x >= 0 && x < W && y >= 0 && y < H && (display[y][x] == '.' || display[y][x] == ' '))
                    display[y][x] = '*';
            }
        }
        REP (k, robots.size()) {
            int x = robots[k].x;
            int y = robots[k].y;
            display[y][x] = 'o';
        }
        for (int i = display.size() - 1; i >= 0; i--) {
            cerr << display[i] << endl;
        }
    }
};

bool operator< (const State& lhs, const State& rhs) {
    return lhs.num_unpainted < rhs.num_unpainted;
}

void printMoves(const vector<Move>& moves) {
    string s;
    for (auto move : moves) {
        s += move.move_type;
    }
    cerr << "Moves: " << s << endl;
}

int getDistanceToNearestDot(const State& state) {
    return 0;
}

vector<tuple<int, int>> findShortestPath(int x, int y, char tgt, const vector<string>& map) {
    const int H = (int)map.size();
    const int W = (int)map[0].size();
    vector<vector<int>> dist(H, vector<int>(W, INF));
    queue<tuple<int, int>> que;
    dist[y][x] = 0;
    que.push(make_tuple(x, y));
    int foundX = -1;
    int foundY = -1;
    while (!que.empty()) {
        int cx = get<0>(que.front());
        int cy = get<1>(que.front());
        que.pop();
        if (map[cy][cx] == tgt) {
            foundX = cx;
            foundY = cy;
            break;
        }
        REP(k, 4) {
            int nx = cx + DX[k];
            int ny = cy + DY[k];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H || map[ny][nx] == '#' || dist[ny][nx] != INF)
                continue;
            dist[ny][nx] = dist[cy][cx] + 1;
            que.push(make_tuple(nx, ny));
        }    
    }
    vector<tuple<int, int>> path;
    if (foundX >= 0) {
        path.push_back(make_tuple(foundX, foundY));
        int d = dist[foundY][foundX];
        while (d > 0) {
            REP(k, 4) {
                int nx = foundX + DX[k];
                int ny = foundY + DY[k];
                if (nx < 0 || nx >= W || ny < 0 || ny >= H || map[ny][nx] == '#' || dist[ny][nx] != d - 1)
                    continue;
                foundX = nx;
                foundY = ny;
                d = d-1;
                path.push_back(make_tuple(nx, ny));
            }
        }
        reverse(path.begin(), path.end());
    }
    cerr << "PATH: ";
    REP(i, path.size()) cerr << "(" << get<0>(path[i]) << "," << get<1>(path[i]) << ")";
    cerr << endl;
    return path;
}

pair<int, vector<vector<bool>>> fillByOwner(
        int sx, int sy, int r_idx, int max_fill_dist,
        vector<vector<int>>& owner_map,
        const State& state) {
    const int H = (int)owner_map.size();
    const int W = (int)owner_map[0].size();
    
    vector<vector<bool>> res(H, vector<bool>(W, false));
    vector<vector<int>> dist(H, vector<int>(W, INF));
    queue<pair<int,int>> que;
    int size = 0;
    int color_to_fill = owner_map[sy][sx];

    dist[sy][sx] = 0;
    res[sy][sx] = true;
    owner_map[sy][sx] = r_idx;
    size++;
    que.push(make_pair(sx, sy));

    while (!que.empty()) {
        int x = que.front().first;
        int y = que.front().second;
        que.pop();
        
        REP(k, 4) {
            int nx = x + DX[k];
            int ny = y + DY[k];
            int nd = dist[y][x] + 1;
            if (nx < 0 || nx >= W || ny < 0 || ny >= H)
                continue;
            if (state.map[ny][nx] == '#')
                continue;
            if (state.painted[ny][nx])
                continue;
            if (dist[ny][nx] != INF)
                continue;
            if (nd > max_fill_dist)
                continue;
            if (owner_map[ny][nx] != color_to_fill)
                continue;

            dist[ny][nx] = nd;
            res[ny][nx] = true;
            owner_map[ny][nx] = r_idx;
            size++;
            que.push(make_pair(nx, ny));
        }
    }
    return make_pair(size, res);;
}

// Each islands represented as (size, nearest_point, the islands)
vector<tuple<int, pair<int,int>, vector<vector<bool>>>> findNotOwnedIslands(
        int sx, int sy, int r_idx, int max_dist, int max_fill_dist,
        vector<vector<int>>& owner_map, const State& state) {

    const int H = (int)owner_map.size();
    const int W = (int)owner_map[0].size();
    vector<tuple<int, pair<int,int>, vector<vector<bool>>>> res;

    vector<vector<int>> dist(H, vector<int>(W, INF));
    queue<pair<int,int>> que;
    dist[sy][sx] = 0;
    que.push(make_pair(sx, sy));
    while (!que.empty()) {
        int x = que.front().first;
        int y = que.front().second;
        que.pop();

        if (!state.painted[y][x] && owner_map[y][x] == -1) {
            auto result = fillByOwner(x, y, r_idx,
                    max_fill_dist, owner_map, state);
            res.push_back(make_tuple(result.first, make_pair(x, y), result.second));
        }
        REP(k, 4) {
            int nx = x + DX[k];
            int ny = y + DY[k];
            int nd = dist[y][x] + 1;
            if (nx < 0 || nx >= W || ny < 0 || ny >= H)
                continue;
            if (state.map[ny][nx] == '#')
                continue;
            if (dist[ny][nx] != INF)
                continue;
            if (nd > max_dist)
                continue;
            
            dist[ny][nx] = nd;
            que.push(make_pair(nx, ny));
        }
    }
    cerr << "==========================================" << endl;
    REP(k, res.size()) {
        cerr << "----------------------- " << k << " ------- " << get<0>(res[k]) << endl;
        for (int i = H-1; i >= 0; i--) {
            REP(j, W) {
                cerr << (get<2>(res[k])[i][j] ? '*' : '-');
            }
            cerr << endl;
        }
    }
    return res;
}

vector<Move> computeBestMovesForP(State state, int r_idx, const RobotOrder& order) {
    constexpr int BEAM_WIDTH = 15;
    constexpr int BEAM_DEPTH = 15;
    const int H = (int)state.map.size();
    const int W = (int)state.map[0].size();

    // Beam search
    
    REP(i, H) {
        REP(j, W) {
            if (!state.painted[i][j]) {
                if (!order.target_area[i][j])
                    state.painted[i][j] = true;
            }
        }
    }

    state.computeCellScores();
    vector<vector<tuple<double, State, vector<Move>>>> best_states(
            BEAM_DEPTH+1, vector<tuple<double, State, vector<Move>>>());
    best_states[0].push_back(
            make_tuple(state.score(), state, vector<Move>()));

    for (int k = 0; k < BEAM_DEPTH; k++) {
        for (int i = 0; i < (int)best_states[k].size(); i++) {
            State cur_state = get<1>(best_states[k][i]);
            vector<Move> cur_moves = get<2>(best_states[k][i]);
            for (auto move_type : MOVE_TYPES) {
                if (!cur_state.can(r_idx, move_type))
                    continue;
                if (cur_state.B > 0 && move_type != 'B') // Use B immediately.
                    continue;
                auto moved = cur_state.move(r_idx, move_type);
                State next_state = moved.second;
                Move move = moved.first;
                vector<Move> next_moves = cur_moves;
                next_moves.push_back(move);
                double next_score = next_state.score();
                best_states[k+1].push_back(
                        make_tuple(next_score, next_state, next_moves));
            }

            std::sort(best_states[k+1].rbegin(), best_states[k+1].rend());
            if (best_states[k+1].size() > BEAM_WIDTH) {
                best_states[k+1].erase(best_states[k+1].begin() + BEAM_WIDTH, best_states[k+1].end());
            }
            if (get<1>(best_states[k+1][0]).finished()) {
                return get<2>(best_states[k+1][0]);
            }
        }
    }
    if (best_states[BEAM_DEPTH].empty()) {
        cerr << "ERROR!! best_states[BEAM_DEPTH] is empty." << endl;
        return vector<Move>();
    }
    if (get<0>(best_states[BEAM_DEPTH][0]) == get<0>(best_states[0][0])) {
        cerr << "WARNING!! No progress. " << get<0>(best_states[BEAM_DEPTH][0]) << endl;
    }
    return get<2>(best_states[BEAM_DEPTH][0]);
}


vector<RobotOrder> computeRobotOrders(const State& state, const vector<RobotOrder>& prev_robot_orders) {
    int H = (int)state.map.size();
    int W = (int)state.map[0].size();
    int R = (int)state.robots.size();
    
    vector<RobotOrder> robot_orders(prev_robot_orders);
    while ((int)robot_orders.size() < R)
        robot_orders.push_back(RobotOrder());

    //   1. If C > 0, set mode G to a robot which is near X cell.
    int num_robots_for_C = 0;
    REP(k, R) {
        if (robot_orders[k].mode == 'C')
            num_robots_for_C++;
    }
    if (state.C > num_robots_for_C) {
        int available_C = state.C - num_robots_for_C;
        REP(k, R) {
            if (available_C > 0 && robot_orders[k].mode == 'W') {
                available_C--;
                robot_orders[k].mode = 'C';
                robot_orders[k].target_path = findShortestPath(state.robots[k].x, state.robots[k].y, 'X', state.map);
            }
        }
    }
    /*
    REP(k, R) {
        robot_orders[k].print();
    }
    */

    // Temporary: Other free robots paint the area.

#if 0
    // Debug
    REP(k, R) {
        if (robot_orders[k].mode != 'W')
            continue;
        robot_orders[k].mode = 'P';
        vector<vector<bool>> target_area(H, vector<bool>(W, false));
        REP(i, H) REP(j, W) if (state.map[i][j] != '#' && !state.painted[i][j])
            target_area[i][j] = true;
        robot_orders[k].target_area = target_area;
    }
#else
    // 3. For free robots (not in mode G), assign cells to each robots
    // and set up target area.
    vector<vector<int>> owner_map(H, vector<int>(W, -1));
    REP(k, R) {
        if (robot_orders[k].mode != 'W')
            continue;
        int sx = state.robots[k].x;
        int sy = state.robots[k].y;
        auto islands = findNotOwnedIslands(sx, sy, k, 9, 15, owner_map, state);
        sort(islands.begin(), islands.end());
        cerr << "---------------------" << endl;
        REP(i, islands.size()) {
            cerr << "size: " << get<0>(islands[i]);
        }
        for (int i = 1; i < (int)islands.size(); i++) { // choose the smallest islands and release others.
            fillByOwner(get<1>(islands[i]).first, get<1>(islands[i]).second, -1, 15, owner_map, state);
        }
        if (islands.size() > 0) {
            robot_orders[k].mode = 'P';
            robot_orders[k].target_area = get<2>(islands[0]);
            robot_orders[k].turn_to_expire = 4;
        } else {
            // Get boosters.


            /* 
            robot_orders[k].mode = 'C';
            robot_orders[k].target_path = findShortestPath(state.robots[k].x, state.robots[k].y, 'X', state.map);
            */
            // debug
            robot_orders[k].mode = 'P';
            vector<vector<bool>> target_area(H, vector<bool>(W, false));
            REP(i, H) REP(j, W) if (state.map[i][j] != '#' && !state.painted[i][j])
                target_area[i][j] = true;
            robot_orders[k].target_area = target_area;
        }
    }
#endif

#if 0
    // 3-1. Compute domination.
    set<tuple<int, int>> start;
    vector<vector<int>> domination(H, vector<int>(W, -1));
    queue<tuple<int, int, int>> que;
    REP(k, state.robots.size()) {
        int x = state.robots[k].x;
        int y = state.robots[k].y;
        if (start.find(make_tuple(x, y)) != start.end()) {
            bool found = false;
            for (int dx = -1; dx <= 1 && !found; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= W || ny < 0 || ny >= H)
                        continue;
                    if (start.find(make_tuple(nx, ny)) != start.end())
                        continue;
                    if (state.map[ny][nx] == '#')
                        continue;
                    x = nx;
                    y = ny;
                    found = true;
                    break;
                }
            }
            if (!found) {
                cerr << "###### Critical error!" << endl;
            }
        }
        start.insert(make_tuple(x, y));
        domination[y][x] = k;
        que.push(make_tuple(k, x, y));
    }
    while (!que.empty()) {
        int c = get<0>(que.front());
        int x = get<1>(que.front());
        int y = get<2>(que.front());
        que.pop();
        REP(k, 4) {
            int nx = x + DX[k];
            int ny = y + DY[k];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H)
                continue;
            if (domination[ny][nx] != -1)
                continue;
            if (state.map[ny][nx] == '#')
                continue;
            domination[ny][nx] = c;
            que.push(make_tuple(c, nx, ny));
        }
    }
#endif
    
#if 0
    cerr << "###### Print domination map ######" << endl;
    for (int i = H-1; i >= 0; i--) {
        for (int j = 0; j < W; j++) {
            if (domination[i][j] < 0)
                cerr << ' ';
            else
                cerr << domination[i][j];
        }
        cerr << endl;
    }
#endif

#if 0
    // 3-2. Find nearby islands.
    // Experiment: Set all area 
    REP(k, R) {
        // Temporary. Discard this when necessary to reduce copy cost.
        robot_orders[k].target_area = vector<vector<bool>>(H, vector<bool>(W, false));
    }
    REP(i, H) {
        REP(j, W) {
            if (state.map[i][j] != '#' && !state.painted[i][j]) {
                assert(domination[i][j] >= 0);
                int robot_idx = domination[i][j];
                robot_orders[robot_idx].target_area[i][j] = true;
            }
        }
    }

    // Temporary
    REP(k, R) {
        if (robot_orders[k].mode == 'W')
            robot_orders[k].mode = 'P';
    }
#endif
    
    // ####### Update target area for each robot
    // Restting to mode P temporarily.

#if 0
    REP(k, R) {
        cerr << "------ Robot " << k << " -------" << endl;
        for (int i = H-1; i >= 0; i--) {
            REP(j, W) {
                if (state.map[i][j] == '#')
                    cerr << '#';
                else {
                    cerr << (robot_orders[k].target_area[i][j] ? '.' : ' ');
                }
            }
            cerr << endl;
        }
    }
#endif
    return robot_orders;
}

vector<RobotOrder> updateAfterMoves(const State& state, 
        const vector<RobotOrder>& prev_robot_orders, const vector<Move>& moves) {
    vector<RobotOrder> robot_orders(prev_robot_orders);
    int H = (int)state.map.size();
    int W = (int)state.map[0].size();
    int R = (int)state.robots.size();
    REP(k, robot_orders.size()) {
        if (robot_orders[k].mode == 'C' && moves[k].move_type == 'C')
            robot_orders[k].mode = 'W';
        if (robot_orders[k].mode == 'P') {
            int remaining = 0;
            REP(i, H) REP(j, W) {
                if (robot_orders[k].target_area[i][j]) {
                    if (state.painted[i][j])
                        robot_orders[k].target_area[i][j] = false;
                    else
                        remaining++;
                }
            }
            robot_orders[k].turn_to_expire--;
            if (remaining == 0 || robot_orders[k].turn_to_expire <= 0) {
                robot_orders[k].mode = 'W';
                robot_orders[k].target_area.clear();
            }
        }
    }
    return robot_orders;    
}

vector<Move> computeBestMoves(const State& state, const vector<RobotOrder>& robot_orders) {
    int H = (int)state.map.size();
    int W = (int)state.map[0].size();
    int R = (int)state.robots.size();
    /*    
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> rand_dist(0, 3);
    */

    vector<Move> robot_moves(R);

    // Get next move of each robots. (If they're not using boosters)
    REP(k, R) {
        if (robot_orders[k].mode == 'C') {
            if (state.can(k, 'C')) {
                robot_moves[k] = Move('C');
            } else {
                int path_idx = 0;
                REP(i, robot_orders[k].target_path.size()) {
                    if (state.robots[k].x == get<0>(robot_orders[k].target_path[i]) &&
                            state.robots[k].y == get<1>(robot_orders[k].target_path[i])) {
                        path_idx = i;
                        break;
                    }
                }
                char direction;
                REP(i, 4) {
                    int nx = state.robots[k].x + DX[i];
                    int ny = state.robots[k].y + DY[i];
                    if (nx == get<0>(robot_orders[k].target_path[path_idx+1]) && 
                            ny == get<1>(robot_orders[k].target_path[path_idx+1])) {
                        direction = DXY_DIR[i];
                        break;
                    }
                }
                robot_moves[k] = Move(direction);
            }
        }
        if (robot_orders[k].mode == 'P') {
            vector<Move> moves = computeBestMovesForP(state, k, robot_orders[k]);
            robot_moves[k] = moves[0];
        }
        if (robot_orders[k].mode == 'W') {
            robot_moves[k] = Move('Z');
        }
    }
    return robot_moves;
}

vector<vector<Move>> solve(const State& initial_state) {
    State state = initial_state;
    vector<vector<Move>> history;
    vector<RobotOrder> robot_orders;
    while (!state.finished()) {
        robot_orders = computeRobotOrders(state, robot_orders);
        vector<Move> robot_moves = computeBestMoves(state, robot_orders);
        while (history.size() < robot_moves.size())
            history.push_back(vector<Move>());
        state.enableReservedBoosters();
        REP(k, robot_moves.size()) {
            auto move_result = state.move(k, robot_moves[k].move_type);
            state = move_result.second;
            history[k].push_back(move_result.first);
        }
        robot_orders = updateAfterMoves(state, robot_orders, robot_moves);
        state.print(robot_orders);
    }
    return history;
}

string output(const vector<vector<Move>>& history) {
    std::string res;
    const int R = history.size();
    REP(k, R) {
        for (auto move : history[k]) {
            res += move.move_type;
            if (move.move_type == 'B') {
                stringstream ss;
                ss << "(" << move.dx << "," << move.dy << ")";
                res += ss.str();
            }
        }
        if (k != R-1)
            res += '#';
    }
    return res;
}

int main(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    std::string initial_boosters;
    if (argc >= 2)
        initial_boosters = std::string(argv[1]);

    std::string input = "";
    std::getline(std::cin, input);
    std::vector<std::string> parts = splitAll(input, "#");

    Mine mine;
    mine.parse(parts[0]);
    mine.parseObstacles(parts[2]);
    std::tuple<int, int> initial_pos = parsePoint(parts[1]);
    mine.setPos(std::get<0>(initial_pos), std::get<1>(initial_pos));
    mine.boosters = parts[3];

    State state(mine, initial_boosters);
    state.print();
    vector<vector<Move>> history = solve(state);
    std::string answer = output(history);
    std::cout << answer << std::endl;
    std::cerr << "T: " << history[0].size();

    return 0;
}
