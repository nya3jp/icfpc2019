// Atoha di-pu ra-ningu de

#include <iostream>
#include <fstream>
#include <string>
#include "libgen.h"
#include "string.h"
#include <random>
#include <cassert>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "glog/logging.h"

#include "psh-solver/simulator.h"

using namespace std;
using namespace icfpc2019;

std::string ReadFile(const std::string path)
{
  std::ifstream fin(path);
  CHECK(fin);

  return std::string(
    std::istreambuf_iterator<char>(fin),
    std::istreambuf_iterator<char>());
}

enum {
  MOVE_W = 0,
  MOVE_S,
  MOVE_A,
  MOVE_D,
  TURN_Q,
  TURN_E,
  BOOST_F,
  BOOST_L,
  BOOST_R,
  BOOST_T,
  GIVEUP_BFS,
  ACTION_NR
};

enum visual_layer_id {
  LV_BBOX = 0, // Bounding box
  LV_WALL, // Wall
  LV_UNWRAP, // Not wrapped
  LV_WRAPPER, // Other wrappers
  LV_MANIPS,  // Current manipulator shape
  LV_B_MANIP, // Booster(Manipulator, B)
  LV_B_FAST,  // Booster(fastwheel, F)
  LV_B_DRILL, // Booseter(drill, L)
  LV_B_TELEPORT, // Booster(teleport, R)
  LV_B_CLONE, // Booster(clone, C)
  LV_B_CLONEPAD, // Cloning pad (X)
  LV_NR // end of visual layers
};

// need to use this structure because of dreadful clone.
struct wrapper_num {
  size_t wrapper_id;
  size_t num_all_wrappers;
};

Point find_attach_point(const Wrapper& w)
{
  const auto manipulators = w.manipulators();
  // I will regret this hack.
  Point diff = manipulators[manipulators.size() - 1] - manipulators[manipulators.size() - 2];
  return manipulators[manipulators.size() - 1] + diff;
}

// returns next wrapper number (update after sending telemetry)
wrapper_num do_action(int action, const wrapper_num& wn, Map* map, int *reward, int* terminated, ostringstream *oss)
{
  Instruction i;
  const auto me = map->wrappers()[wn.wrapper_id];

  // prioritized action
  if(map->collectedB() > 0) {
    i.type = Instruction::Type::B;
    // add to rightmost
    i.arg = find_attach_point(map->wrappers()[wn.wrapper_id]);
  }else if(map->collectedC() > 0 &&
           map->GetBooster(me.point()) == absl::make_optional(Booster::X)) {
    i.type = Instruction::Type::C;
  }else {
    switch(action) {
    case MOVE_W:
      i.type = Instruction::Type::W;
      break;
    case MOVE_S:
      i.type = Instruction::Type::S;
      break;
    case MOVE_A:
      i.type = Instruction::Type::A;
      break;
    case MOVE_D:
      i.type = Instruction::Type::D;
      break;
    case TURN_Q:
      i.type = Instruction::Type::Q;
      break;
    case TURN_E:
      i.type = Instruction::Type::E;
      break;
    case BOOST_F:
      i.type = Instruction::Type::F;
      break;
    case BOOST_L:
      i.type = Instruction::Type::L;
      break;
    case BOOST_R:
      i.type = Instruction::Type::R;
      break;
    case BOOST_T:
      i.type = Instruction::Type::Z;
      // TODO
      break;
    case GIVEUP_BFS:
      // TODO
      i.type = Instruction::Type::Z;
      break;
    }
  }

  // commit.
  int rbefore = map->remaining();
  Map::RunResult r = map->Run(wn.wrapper_id, i);
  // Run is indeed implemented by DryRun() and then RunUnsafe so this should be OK
  wrapper_num retwn = wn;
  *terminated = 0;
  
  switch(r) {
  case Map::RunResult::SUCCESS:
    // only in this case map is updated
    {
      int rafter = map->remaining();
      *reward = (rbefore - rafter);
      if(rafter == 0) {
        *terminated = 1;
      }
    }

    {
      (*oss) << i;
    }

    // update wrapper id
    retwn.wrapper_id++;
    if(retwn.wrapper_id == retwn.num_all_wrappers){
      retwn.wrapper_id = 0;
      retwn.num_all_wrappers = map->wrappers().size();
    }
    break;
  case Map::RunResult::NO_WRAPPER:
  case Map::RunResult::OUT_OF_MAP:
  case Map::RunResult::WALL:
  case Map::RunResult::NO_BOOSTER:
  case Map::RunResult::BAD_CLONE_POSITION:
    *reward = -1;
    break;
    
  case Map::RunResult::UNKNOWN_TELEPORT_POSITION:
  case Map::RunResult::BAD_TELEPORT_POSITION:
  case Map::RunResult::BAD_MANIPULATOR_POSITION:
  case Map::RunResult::UNKNOWN_INSTRUCTION:
    cerr << "RunResult " << (int) r << endl;
    cerr.flush();
    assert(false);
    break;
  }
  return retwn;
}

// This wn should be old one because wrapper_id is total mess when C is exec'ed
void report_state(const Map &map, const wrapper_num &wn)
{
  const auto me = map.wrappers()[wn.wrapper_id];
  // encode the map around me
  Point center = me.point();
  const int sleeve = 6;
  const int dim = sleeve * 2 + 1;
  // 13x13x11 map. I know this is a brain-dead code and actually a bitmap should be suffice ...
  vector<vector<vector<int>>> binmap(LV_NR, vector<vector<int>>(dim, vector<int>(dim, 0)));

  for(int x = -sleeve; x <= sleeve; x++){
    int xi = x + sleeve;
    for(int y = -sleeve; y <= sleeve; y++) {
      int yi = y + sleeve;
      Point p = center + Point({x, y});
      if(!map.InMap(p)) {
        binmap[LV_BBOX][yi][xi] = 1;
      }else{
        Cell c = map[p];
        if(c == Cell::EMPTY) {
          binmap[LV_UNWRAP][yi][xi] = 1;
        }else if(c == Cell::WALL) {
          binmap[LV_WALL][yi][xi] = 1;
        }
        auto booster = map.GetBooster(p);
        if(booster) {
          if(booster.value() == Booster::B) {
            binmap[LV_B_MANIP][yi][xi] = 1;
          }else if(booster.value() == Booster::F) {
            binmap[LV_B_FAST][yi][xi] = 1;
          }else if(booster.value() == Booster::L) {
            binmap[LV_B_DRILL][yi][xi] = 1;
          }else if(booster.value() == Booster::X) {
            binmap[LV_B_CLONEPAD][yi][xi] = 1;
          }else if(booster.value() == Booster::R) {
            binmap[LV_B_TELEPORT][yi][xi] = 1;
          }else if(booster.value() == Booster::C) {
            binmap[LV_B_CLONE][yi][xi] = 1;
          }
        }
        for(size_t i = 0; i < wn.num_all_wrappers; ++i) {
          if(i == wn.wrapper_id) continue;
          Point other = map.wrappers()[i].point();
          if(other == p){
            binmap[LV_WRAPPER][yi][xi] = 1;
          }
        }
      }
    }
  }
  for(const auto m: me.manipulators()) {
    int yi = m.y + sleeve;
    int xi = m.x + sleeve;
    binmap[LV_MANIPS][yi][xi] = 1;
  }

  for(int mapid = 0; mapid < LV_NR; mapid++) {
    for(int y = 0; y < dim; ++y) {
      for(int x = 0; x < dim; ++x) {
        char c = static_cast<char>(binmap[mapid][y][x] ? 255 : 0);
        fwrite(&c, 1, 1, stdout);
      }
    }
  }

  // write down other states
  // B's always used on sight!
  {
    char c;
    c = static_cast<char>(map.collectedF());
    fwrite(&c, 1, 1, stdout);
    c = static_cast<char>(map.collectedL());
    fwrite(&c, 1, 1, stdout);
    c = static_cast<char>(map.collectedR());
    fwrite(&c, 1, 1, stdout);
    c = static_cast<char>(map.collectedC());
    fwrite(&c, 1, 1, stdout);
  }
  const char* endrepr = ".";
  fwrite(endrepr, 1, 1, stdout);
  fflush(stdout);
}

int main(int argc, char* argv[])
{
  string dir = dirname(argv[0]);
  string problemlist = dir + "/problems.in";

  cerr << "probf" << problemlist << endl;

  vector<string> problems;
  {
    ifstream ifs(problemlist.c_str());
    if(!ifs) {
      cerr << "Failed to read problem list" << endl;
      return 2;
    }
    string fname;
    while(ifs >> fname) {
      problems.push_back(fname);
    }
  }

  mt19937 randgen(1);
  uniform_int_distribution<> problem_selector(0, problems.size() - 1);

  cerr << problems.size()<< " problems" << endl;

  unique_ptr<icfpc2019::Map> pmap;
  wrapper_num wn({0, 1});
  string curprob;
  ostringstream cmdbuf;
  ofstream resultsave("save.txt");
  while(true){
    char buf[16];
    int r = fread(&buf[0], 1, 1, stdin);
    if(r != 1) break;
    switch(buf[0]) {
    case '@':
      // reset
      {
        fwrite("@", 1, 1, stdout);
        fflush(stdout);
        int randprob = problem_selector(randgen);
        curprob = problems[randprob];
        auto desc = icfpc2019::ParseDesc(ReadFile(curprob));
        pmap.reset(new icfpc2019::Map(desc));
        report_state(*pmap, wn);
        cmdbuf.str("");
        cmdbuf.clear();
      }
      break;
    case 'Q':
      // close
      exit(0);
      break;
    default:
      // action
      //cerr << "Incoming action " << (int)(buf[0]) << endl;
      cerr.flush();
      if(int(buf[0]) < 0 || int(buf[0] >= ACTION_NR)) {
        // invalid code
        abort();
      }
      {
        int reward;
        int terminated;
        wrapper_num wn_updated;
        wn_updated = do_action(int(buf[0]), wn, pmap.get(), &reward, &terminated, &cmdbuf);
        //cerr << "Action done" << endl;
        cerr.flush();
        fwrite(&reward, sizeof(reward), 1, stdout);
        fwrite(&terminated, sizeof(terminated), 1, stdout);
        if(terminated) {
          cerr << "Cleared map " << curprob << ": " << pmap->num_steps() << endl;
          resultsave << curprob << " " << pmap->num_steps() << endl;
          resultsave << cmdbuf.str() << endl;
        }
        report_state(*pmap, wn);
        wn = wn_updated;
      }
    }
  }
  return 0;
}


/*
  +Instruction Solver::Paint(const Point& src, const Region& region) {
+    std::queue<std::tuple<Point, Instruction>> q;
+    Visit(src);
+
+    for (const Dir dir : kDirs) {
+        const Point p1 = src + Delta(dir);
+        if (cell(p1) != Cell::WALL) {
+            q.push(std::make_tuple(p1, ToInstruction(dir)));
+            Visit(p1);
+        }
+    }
+
+    while (!q.empty()) {
+        const auto e = q.front();
+        q.pop();
+
+        const auto& p0 = std::get<Point>(e);
+
+        if (cell(p0) == Cell::EMPTY && region.Contains(p0)) {
+            return std::get<Instruction>(e);
+        }
+
+        for (const Dir dir : kDirs) {
+            const Point p1 = p0 + Delta(dir);
+            if (cell(p1) != Cell::WALL && !visited(p1)) {
+                q.push(std::make_tuple(p1, std::get<Instruction>(e)));
+                Visit(p1);
+            }
+        }
+    }
+
+    return {Instruction::Type::Z};  // Oops...
+}

 */
