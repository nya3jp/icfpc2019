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

struct Move {
    char move_type;
    int dx;
    int dy;
};

bool operator< (const Move& lhs, const Move& rhs) {
    return lhs.move_type < rhs.move_type;
}

struct State {
    std::vector<std::string> map;
    std::vector<std::vector<bool>> painted;
    std::vector<tuple<int, int>> arms;
    int cur_x;
    int cur_y;
    int B, F, L, R, C;
    int tB, tF;
    int gotB, gotF;

    // Computed states
    int num_unpainted;
    vector<vector<int>> cell_scores;

    State(const Mine& mine, const string boosters) {
        int W = mine.max_x, H = mine.max_y;
        cur_x = mine.cur_x;
        cur_y = mine.cur_y;
        B = F = L = R = C = 0;
        tB = 0;
        tF = 0;
        gotB = 0;
        gotF = 0;
        for (char c : boosters) {
            if (c == 'B') B++;
            if (c == 'F') F++;
            if (c == 'L') L++;
            if (c == 'R') R++;
            if (c == 'C') C++;
        }
        
        map = std::vector<std::string>(H, std::string(W, '.'));
        painted = std::vector<std::vector<bool>>(H, std::vector<bool>(W, false));
        arms.push_back(make_tuple(0, 0));
        arms.push_back(make_tuple(1, 0));
        arms.push_back(make_tuple(1, -1));
        arms.push_back(make_tuple(1, 1));

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
        for (auto arm : arms) {
            int x = cur_x + get<0>(arm);
            int y = cur_y + get<1>(arm);
            if (x >= 0 && x < (int)map[0].size() && y >= 0 && y < (int)map.size()) {
                painted[y][x] = true;
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
        for (int i=0; i<cell_scores.size(); i++) {
            for (int j=0; j<cell_scores[i].size(); j++) {
                if (map[i][j] == '#' || painted[i][j])
                    continue;
                int adjacent_nontarget = 0;
                for (int k=0; k<4; k++) {
                    int nr = i + DY[k];
                    int nc = j + DX[k];
                    if (nr >= 0 && nr < cell_scores.size() && nc >= 0 && nc < cell_scores[0].size()) {
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
        int dist_to_nearest = INF;
        queue<tuple<int, int, int>> que;
        vector<vector<bool>> visited(map.size(), vector<bool>(map[0].size(), false));
        visited[cur_y][cur_x] = true;
        que.push(make_tuple(0, cur_x, cur_y));
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
                if (nx < 0 || nx >= map[0].size() || ny < 0 || ny >= map.size())
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

        // Consider boosters
        score += gotF * 10000;
        score += gotB * 10000;

        return score;
    }

    bool can(char move_type) const {
        if (move_type == 'W' || move_type == 'S' || move_type == 'A' || move_type == 'D') {
            int dir_index = 0;
            if (move_type == 'W') dir_index = 1;
            if (move_type == 'A') dir_index = 2;
            if (move_type == 'S') dir_index = 3;
            int next_x = cur_x + DX[dir_index];
            int next_y = cur_y + DY[dir_index];
            return next_x >= 0 && next_x < (int)map[0].size() &&
                next_y >= 0 && next_y < (int)map.size() &&
                map[next_y][next_x] != '#';
        } else if (move_type == 'E' || move_type == 'Q') {
            return true;
        } else if (move_type == 'B') {
            return B > 0;
        } else if (move_type == 'F') {
            return F > 0;
        } else {
            // TODO: support other operations.
            return false;
        }
    }

    pair<Move, State> move(char move_type) const {
        State next_state = *this;
        int b_dx = 0;
        int b_dy = 0;
        if (move_type == 'W' || move_type == 'S' || move_type == 'A' || move_type == 'D') {
            int dir_index = 0;
            if (move_type == 'W') dir_index = 1;
            if (move_type == 'A') dir_index = 2;
            if (move_type == 'S') dir_index = 3;
            int turns = ((tF > 0) ? 2 : 1);
            for (int t=0; t<turns; t++) {
                int next_x = next_state.cur_x + DX[dir_index];
                int next_y = next_state.cur_y + DY[dir_index];
                if (next_x < 0 || next_x >= next_state.map[0].size() ||
                        next_y < 0 || next_y >= next_state.map.size() ||
                        next_state.map[next_y][next_x] == '#')
                    continue;
                next_state.cur_x = next_x;
                next_state.cur_y = next_y;
                if (next_state.map[next_y][next_x] == 'B') {
                    next_state.B++;
                    next_state.gotB++;
                    next_state.map[next_y][next_x] = '.';
                }
                if (map[next_y][next_x] == 'F') {
                    next_state.F++;
                    next_state.gotF++;
                    next_state.map[next_y][next_x] = '.';
                }
                next_state.paint();
            }
        } else if (move_type == 'E') { // clockwize
            for (size_t i = 0; i < arms.size(); i++) {
                int dx = get<0>(arms[i]);
                int dy = get<1>(arms[i]);
                next_state.arms[i] = make_tuple(dy, -dx);
            }
            next_state.paint();
        } else if (move_type == 'Q') { // counter-clockwize
            for (size_t i = 0; i < arms.size(); i++) {
                int dx = get<0>(arms[i]);
                int dy = get<1>(arms[i]);
                next_state.arms[i] = make_tuple(-dy, dx);
            }
            next_state.paint();
        } else if (move_type == 'F') {
            if (next_state.tF > 0)
                next_state.tF += 50;
            else
                next_state.tF = 51;
            next_state.F = F - 1;

        } else if (move_type == 'B') {
            const int BX[] = {-1, -1, -1, 0, 0};
            const int BY[] = {0, 1, -1, 1, -1};
            int k = arms.size() - 4;
            if (k < 5) {
                int bx = BX[k];
                int by = BY[k];
                if (arms[1] == make_tuple(1, 0)) {
                } else if (arms[1] == make_tuple(0, 1)) {
                    bx = -BY[k];
                    by = BX[k];
                } else if (arms[1] == make_tuple(-1, 0)) {
                    bx = -BX[k];
                    by = -BY[k];
                } else {
                    bx = BY[k];
                    by = -BX[k];
                }
                b_dx = bx;
                b_dy = by;
                next_state.arms.push_back(make_tuple(bx, by));
            }
            next_state.B = B - 1;
        }

        next_state.tF = max(0, next_state.tF-1);
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
        cerr << "===============================" << endl;
        cerr << "B:" << B << " F:" << F << endl;
        cerr << "tB:" << tB << " tF:" << tF << endl;
        for (int i = map.size() - 1; i >= 0; i--) {
            for (size_t j = 0; j < painted[i].size(); j++) {
                if (i == cur_y && j == cur_x) {
                    cerr << 'o';
                } else if (map[i][j] == '#' || map[i][j] == 'X' || map[i][j] == 'B' || map[i][j] == 'F' || map[i][j] == 'L' || map[i][j] == 'R' || map[i][j] == 'C') {
                    cerr << map[i][j];
                } else {
                    if (painted[i][j])
                        cerr << ' ';
                    else
                        cerr << '.';
                }
            }
            cerr << endl;
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
    const vector<string>& map = state.map;
    return 0;
}

vector<Move> computeBestMoves(State state) {
    constexpr int BEAM_WIDTH = 10;
    constexpr int BEAM_DEPTH = 10;
    
    // Beam search
    // Beam width: 10 
    // Beam Depth: 5
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
                if (!cur_state.can(move_type))
                    continue;
                if (cur_state.B > 0 && move_type != 'B') // Use B immediately.
                    continue;
                auto moved = cur_state.move(move_type);
                State next_state = moved.second;
                Move move = moved.first;
                vector<Move> next_moves = cur_moves;
                next_moves.push_back(move);
                double next_score = next_state.score();
                best_states[k+1].push_back(
                        make_tuple(next_score, next_state, next_moves));
            }
            /* 
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(best_states[k+1].begin(), best_states[k+1].end(), g);
            */
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
};

vector<Move> solve(const State& initial_state) {
    constexpr int LIMIT = 100;
    int turns = 0;
    State state = initial_state;
    vector<Move> moves;
    while (!state.finished()) {
        vector<Move> best_moves = computeBestMoves(state);
        printMoves(best_moves);
        for (auto move : best_moves) {
            state = state.move(move.move_type).second;
        }
        moves.insert(moves.end(), best_moves.begin(), best_moves.end());
        state.print();
    }
    return moves;
}

string output(const vector<Move> moves) {
    std::string res;
    for (auto move : moves) {
        res += move.move_type;
        if (move.move_type == 'B') {
            stringstream ss;
            ss << "(" << move.dx << "," << move.dy << ")";
            res += ss.str();
        }
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
    vector<Move> moves = solve(state);
    std::string answer = output(moves);
    std::cout << answer << std::endl;
    std::cerr << "T: " << moves.size();

    return 0;
}
