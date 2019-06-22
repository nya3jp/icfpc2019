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
using namespace std;

#define REP(i,n) for(int i=0; i<(int)(n); i++)
#define FOR(i,b,e) for(int i=(b); i<(int)(e); i++)
#define EACH(i,c) for(__typeof((c).begin()) i=(c).begin(); i!=(c).end(); i++)
#define ALL(c) (c).begin(), (c).end()
#define mp make_pair
#define dump(x) cerr << #x << " = " << (x) << endl;
typedef long long ll;
template<class T1,class T2> ostream& operator<<(ostream& o,const pair<T1,T2>& p){return o<<"("<<p.first<<","<<p.second<<")";}
template<class T> ostream& operator<<(ostream& o,const vector<T>& v){o<<"[";for(typename vector<T>::const_iterator i=v.begin();i!=v.end();++i){if (i != v.begin()) o << ", ";o<<(*i);}o<<"]";return o;}
template<class T> ostream& operator<<(ostream& o,const set<T>& s){o<<"{";for(typename set<T>::const_iterator i=s.begin();i!=s.end();++i){if(i!=s.begin())o<<", ";o<<(*i);}o<<"}";return o;}
template<class K,class V> ostream& operator<<(ostream& o,const map<K,V>& m){o<<"{";for(typename map<K,V>::const_iterator i=m.begin();i!=m.end();++i){if(i!=m.begin())o<<", ";o<<i->first<<":"<<i->second;}o<<"}";return o;}
template<class T> ostream& operator<<(ostream& o,const vector<vector<T> >& m){o<<"[\n";for(typename vector<vector<T> >::const_iterator i=m.begin();i!=m.end();++i){o<<"  "<<(*i);o<<(i+1!=m.end()?",\n":"\n");}o<<"]\n";return o;}
string bitstr(int n,int d=0){string r;for(int i=0;i<d||n>0;++i,n>>=1){r+=(n&1)?"1":"0";}reverse(r.begin(),r.end());return r;}

constexpr int INF = 1e9;
constexpr int DY[] = {0, 1, 0, -1};
constexpr int DX[] = {1, 0, -1, 0};
int bNum, eNum, tSize, vMin, vMax, mNum, fNum, dNum, rNum, cNum, xNum;
vector<pair<int, int>> inc_points;
vector<pair<int, int>> exc_points;

void processInput0(string input) {
    REP(i, input.size()) {
        if (input[i] == ',' || input[i] == '(' || input[i] == ')') {
            input[i] = ' ';
        }
    }
    stringstream ss(input);
    ss >> bNum >> eNum >> tSize >> vMin >> vMax >> mNum >> fNum >> dNum >> rNum >> cNum >> xNum;
    /*
    dump(bNum);
    dump(eNum);
    dump(tSize);
    dump(vMin);
    dump(vMax);
    dump(mNum);
    dump(fNum);
    dump(dNum);
    dump(rNum);
    dump(cNum);
    dump(xNum);
    */
}

int countPairs(const std::string& str) {
    int res = 0;
    REP(i, str.size()) {
        if (str[i] == ',')
            res++;
    }
    return (res + 1) / 2;
}

void processInput1(string input) {
    stringstream ss(input);
    std::string token;
    std::vector<std::string> tokens;
    while(std::getline(ss, token, '#')) {
        tokens.push_back(token);
    }
    if (tokens.size() < 3) tokens.push_back(string());

    int N = countPairs(tokens[1]);
    int M = countPairs(tokens[2]);
    REP(i, tokens[1].size()) {
        if (tokens[1][i] == '(' || tokens[1][i] == ')' || tokens[1][i] == ',')
            tokens[1][i] = ' ';
    }
    REP(i, tokens[2].size()) {
        if (tokens[2][i] == '(' || tokens[2][i] == ')' || tokens[2][i] == ',')
            tokens[2][i] = ' ';
    }
    stringstream ss1(tokens[1]);
    REP(i, N) {
        int x, y; ss1 >> x >> y;
        inc_points.push_back(make_pair(x, y));
    }
    stringstream ss2(tokens[2]);
    REP(i, M) {
        int x, y; ss2 >> x >> y;
        exc_points.push_back(make_pair(x, y));
    }
}

void processInput() {
    string input;
    cin >> input;
    processInput0(input);
    processInput1(input);
}

void printMap(const vector<string>& m) {
    cerr << "-------------------------" << endl;
    for (int i = m.size()-1; i >= 0; i--)
        cerr << m[i] << endl;
}


bool canAddCenter(char c0, char c1, char c2, char c3, char c4, char c5, char c6, char c7,  char c8) {
    int num_block = (c0 == '#') + (c1 == '#') + (c2 == '#') +
                    (c3 == '#') + (c4 == '#') + (c5 == '#') +
                    (c6 == '#') + (c7 == '#') + (c8 == '#');
    if (num_block != 3)
        return false;
    if (c4 == 'O')
        return false;

    if ((c0 == '#') + (c1 == '#') + (c2 == '#') == 3)
        return true;
    if ((c6 == '#') + (c7 == '#') + (c8 == '#') == 3)
        return true;
    if ((c0 == '#') + (c3 == '#') + (c6 == '#') == 3)
        return true;
    if ((c2 == '#') + (c5 == '#') + (c8 == '#') == 3)
        return true;

    return false;
}

vector<string> incEdges(const vector<string>& m, int numInc) {
    vector<string> res = m;
    REP(r, res.size() - 2) {
        REP(c, res[0].size() - 2) {
            if (canAddCenter(res[r][c], res[r][c+1], res[r][c+2],
                             res[r+1][c], res[r+1][c+1], res[r+1][c+2],
                             res[r+2][c], res[r+2][c+1], res[r+2][c+2])) {
                res[r+1][c+1] = '#';
                numInc -= 4;
                if (numInc <= 0)
                    return res;
            }
        }
    }
    cout << "########## ERROREEEEEEEEEEEE" << endl;
    return res;
}

vector<pair<int, int>> createPolygon(const vector<string>& m) {
    int w = m[0].size();
    int h = m.size();

    int started = -1;
    int finished = -1;
    int yfirst;
    for (int y = 0; y < h; y++) {
        bool found = false;
        for (int x = 0; x < w; x++) {
            if (m[y][x] != '#') {
                if (started == -1)
                    started = x;
            } else {
                if (started != -1) {
                    finished = x;
                    found = true;
                    yfirst = y;
                    break;
                }
            }
        }
        if (found)
            break;
    }

    vector<pair<int, int>> vertices;
    vertices.emplace_back(started, yfirst);
    vertices.emplace_back(finished, yfirst);

    auto prev_direction = make_pair(1, 0);
    auto new_direction = make_pair(0, 1);

    while (true) {
        if (vertices.back() == vertices.front())
            break;
        auto pos = vertices.back();
        int count = 0;
        while (true) {
            int nx = pos.first + new_direction.first;
            int ny = pos.second + new_direction.second;
            const int dX[] = {0, -1, -1, 0};
            const int dY[] = {0, 0, -1, -1};
            count = 0;
            REP(k, 4) {
                if (m[ny + dY[k]][nx + dX[k]] == '#')
                    count++;
            }
            pos = make_pair(nx, ny);
            if (count == 1 || count == 3)
                break;
        }
        vertices.push_back(pos);
        prev_direction = new_direction;
        if (count == 1)
            new_direction = make_pair(prev_direction.second, -prev_direction.first);
        else
            new_direction = make_pair(-prev_direction.second, prev_direction.first);
    }
    //reverse(vertices.begin(), vertices.end());
    vertices.erase(vertices.end()-1);
    return vertices;
}

bool validate(const vector<string>& m) {
    //vector<string> m2 = m;
    // Fill space by BFS
    int sx = inc_points[0].first + 1;
    int sy = inc_points[0].second + 1;
    vector<vector<int>> depth(m.size(), vector<int>(m[0].size(), INF));
    queue<pair<int, int>> que;
    depth[sy][sx] = 0;
    que.push(make_pair(sx, sy));
    while (!que.empty()) {
        int cx = que.front().first;
        int cy = que.front().second;
        que.pop();
        REP(k, 4) {
            int nx = cx + DX[k];
            int ny = cy + DY[k];
            int nd = depth[cy][cx] + 1;
            if (nx < 0 || nx >= m[0].size() || ny < 0 || ny >= m.size())
                continue;
            if (m[ny][nx] == '#')
                continue;
            
            if (depth[ny][nx] > nd) {
                depth[ny][nx] = nd;
                que.push(make_pair(nx, ny));
            }
        }
    }

    // Check if inc_points are included.
    REP(i, inc_points.size()) {
        int x = inc_points[i].first + 1;
        int y = inc_points[i].second + 1;
        if (depth[y][x] == INF)
            return false;
    }
    // Check if exc_points are excluded.
    REP(i, exc_points.size()) {
        int x = exc_points[i].first + 1;
        int y = exc_points[i].second + 1;
        if (m[y][x] != '#')
            return false;
    }
    // Check the number of vertices of the map.
    auto vertices = createPolygon(m);
    if (vertices.size() < vMin || vertices.size() > vMax)
        return false;

    return true;
}

pair<int, int> pickEmpty(const vector<string>& m) {
    int H = m.size();
    int W = m[0].size();

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> distH(1, H-2);
    std::uniform_int_distribution<std::mt19937::result_type> distW(1, W-2);
    while (true) {
        int y = distH(rng);
        int x = distW(rng);
        if (m[y][x] == '.') {
            return make_pair(x, y);
        }
    }
    cerr << "########ERRORORORORORORO" << endl;
    return make_pair(-1, -1);
}

void output(vector<string> m) {
    ostringstream oss;
    // map
    vector<pair<int, int>> vertices = createPolygon(m);
    REP(i, vertices.size()) {
        vertices[i] = make_pair(vertices[i].first - 1, vertices[i].second - 1);
        oss << "(" << vertices[i].first << "," << vertices[i].second << ")";
        if (i != vertices.size()-1)
            oss << ",";
    }
    oss << "#";
    auto start_pos = pickEmpty(m);
    m[start_pos.second][start_pos.first] = 'S';
    oss << "(" << (start_pos.first - 1) << "," << (start_pos.second - 1) << ")";
    oss << "##";
    REP(i, mNum) {
        auto pos = pickEmpty(m);
        m[pos.second][pos.first] = 'B';
        oss << "B(" << (pos.first-1) << "," << (pos.second-1) << ");";
    }
    REP(i, fNum) {
        auto pos = pickEmpty(m);
        m[pos.second][pos.first] = 'F';
        oss << "F(" << (pos.first-1) << "," << (pos.second-1) << ");";
    }
    REP(i, dNum) {
        auto pos = pickEmpty(m);
        m[pos.second][pos.first] = 'L';
        oss << "L(" << (pos.first-1) << "," << (pos.second-1) << ");";
    }
    REP(i, xNum) {
        auto pos = pickEmpty(m);
        m[pos.second][pos.first] = 'X';
        oss << "X(" << (pos.first-1) << "," << (pos.second-1) << ");";
    }
    REP(i, cNum) {
        auto pos = pickEmpty(m);
        m[pos.second][pos.first] = 'C';
        oss << "C(" << (pos.first-1) << "," << (pos.second-1) << ");";
    }
    REP(i, rNum) {
        auto pos = pickEmpty(m);
        m[pos.second][pos.first] = 'R';
        oss << "R(" << (pos.first-1) << "," << (pos.second-1) << ");";
    }
    string str = oss.str();
    if (str[str.size()-1] == ';') {
        str = str.substr(0, str.size()-1);
    }
    cout << str;
}

vector<string> generateMap() {
    vector<string> m(tSize+2, string(tSize+2, '.'));
    REP(i, m[0].size()) {
        m[0][i] = '#';
        m[m[0].size()-1][i] = '#';
    }
    REP(i, m.size()) {
        m[i][0] = '#';
        m[i][m.size()-1] = '#';
    }
    REP(i, inc_points.size()) {
        m[inc_points[i].second+1][inc_points[i].first+1] = 'O';
    }
    REP(i, exc_points.size()) {
        m[exc_points[i].second+1][exc_points[i].first+1] = 'X';
    }

    // Randominze order of exc_points.
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(exc_points.begin(), exc_points.end(), g);

    REP(i, exc_points.size()) {
        int mx = exc_points[i].first + 1;
        int my = exc_points[i].second + 1;
        if (m[my][mx] == '#')
            continue;
        vector<vector<int>> depth(m.size(), vector<int>(m[0].size(), INF));
        depth[my][mx] = 0;
        queue<tuple<int, int, int>> que;
        que.push(make_tuple(depth[my][mx], mx, my));
        bool found = false;
        while (!que.empty() && !found) {
            auto data = que.front();
            que.pop();
            REP(k, 4) {
                int cd = get<0>(data);
                int cx = get<1>(data);
                int cy = get<2>(data);
                int nd = cd + 1;
                int nx = cx + DX[k];
                int ny = cy + DY[k];
                //cout << "nx: " << nx << ", ny: " << ny << endl;
                if (nx < 0 || nx >= m[0].size() || ny < 0 || ny >= m.size())
                    continue;
                if (m[ny][nx] == 'O')
                    continue;

                if (m[ny][nx] == '#') {
                    int current_depth = nd;
                    int current_x = nx;
                    int current_y = ny;
                    while (current_depth > 0) {
                        REP(l, 4) {
                            int next_x = current_x + DX[l];
                            int next_y = current_y + DY[l];
                            if (next_x >= 0 && next_x < m[0].size() &&
                                    next_y >= 0 && next_y < m.size() &&
                                    depth[next_y][next_x] == current_depth - 1) {
                                m[next_y][next_x] = '#';
                                current_x = next_x;
                                current_y = next_y;
                                current_depth--;
                                break;
                            }
                        }
                    }
                    found = true;
                    break;
                }
                if (depth[ny][nx] > nd) {
                    depth[ny][nx] = nd;
                    que.push(make_tuple(nd, nx, ny));
                }
            }
        }
    }
  
    vector<pair<int, int>> vertices = createPolygon(m);
    while (vertices.size() < vMin) {
        m = incEdges(m, vMin - vertices.size() + (vMax - vMin) / 4);
        vertices = createPolygon(m);
    }
    return m;
}

int main() {
    processInput();
    while (true) {
        vector<string> m = generateMap();
        if (validate(m)) {
            output(m);
            break;
        } else {
            cerr << "Failed to validate the map. Re-generating..." << endl;
        }
    }
    return 0;
}

