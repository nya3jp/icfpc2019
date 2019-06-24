#include<iostream>
#include<fstream>
#include<sstream>
#include<istream>
#include<string>
#include<vector>
#include<algorithm>
#include<map>
#include<set>
#include<cmath>
#include<queue>
#include<functional>

#define DPMAX 1000000
#define PROBMAX 300

using namespace std;

struct TestCase{
  string origLine;
  string probID;
  string Purch;
  string submitID;
  string aiID;
  int cost;
  int step;
  double value;
  TestCase(string orig, string prob, string purch, string submit, string ai, string st) :
      origLine(orig), probID(prob), Purch(purch), submitID(submit), aiID(ai), cost(0), step(stoi(st)), value(0.0){
    for(int i=0;i<purch.length();i++){
      switch (purch[i]) {
        case 'B':
          cost += 1000;
          break;
        case 'F':
          cost += 300;
          break;
        case 'L':
          cost += 700;
          break;
        case 'R':
          cost += 1200;
          break;
        case 'C':
          cost += 2000;
          break;
      }
    }
  }
  string print_debug(){
    return "problem:"+probID+" Item:"+Purch+" submitID:" + submitID +
          " aiID:"+aiID+" cost:"+to_string(cost)+" step:"+to_string(step) +
          " value:"+to_string(value);
  }
};

bool operator < (const TestCase a, const TestCase b){
  if(a.probID != b.probID){
    return a.probID < b.probID;
  } else if(a.cost != b.cost){
    return a.cost > b.cost;
  } else {
    return a.step < b.step;
  }
}

vector<TestCase> testcases;
map<string, double> size;
double dp[DPMAX][2];
int updated[DPMAX];
int prevstring[DPMAX];

void inputSolution(string solutionPath){
  string tmpline;
  ifstream ifs;
  ifs.open(solutionPath);
  while(ifs>>tmpline){
    stringstream ss{tmpline};
    string prob; getline(ss, prob, ',');
    string purch; getline(ss, purch, ',');
    string submit; getline(ss, submit, ',');
    string ai; getline(ss, ai, ',');
    string st; getline(ss, st, ',');
    testcases.push_back(TestCase(tmpline, prob, purch, submit, ai, st));
  }
  ifs.close();
  sort(testcases.begin(),testcases.end());
  //for(auto i: testcases){cout<<i.print_debug()<<endl;}
}

void inputProbSize(string probsizePath){
  string tmpline;
  ifstream ifs;
  ifs.open(probsizePath);
  while(getline(ifs,tmpline)){
    stringstream ss{tmpline};
    string probID;
    double x,y;
    ss>>probID>>x>>y;
    size[probID] = log2(x*y)*1000.0;
    //cerr<<probID<<" "<<size[probID]<<endl;
  }
}

void input(string solutionPath, string probsizePath){
  inputSolution(solutionPath);
  inputProbSize(probsizePath);
}

void setupValue_selfratio(string debugfile){
  map<string, int> baseScore;
  for(auto i: testcases){
    if(i.cost == 0){
      if (baseScore.find(i.probID) != baseScore.end()){
        baseScore[i.probID] = min(i.step, baseScore[i.probID]);
      } else {
        baseScore[i.probID] = i.step;
      }
    }
  }
  ofstream ofs;
  ofs.open(debugfile.c_str(), ios::out);
  for(auto &i: testcases){
    double value = ceil(size[i.probID]);
    value -= ceil(size[i.probID]*baseScore[i.probID]/i.step);
    i.value = -value;
    ofs<<i.print_debug()<<endl;
  }
  ofs.close();
}

void setupValue_prevbestratio(string debugfile){
  map<string, int> baseScore;
  string tmpline;
  ifstream ifs;
  ifs.open("bestscore.txt");
  while(getline(ifs,tmpline)){
    int prob,steps;
    char str[10];
    sscanf(tmpline.c_str(), "%d, %d, %s",&prob, &steps, str);
    string probIDstr = to_string(prob);
    while(probIDstr.length() < 3) probIDstr = "0" + probIDstr;
    probIDstr = "prob-"+probIDstr;
    //cout<<prob<<" "<<steps<<" "<<str<<" "<<probIDstr<<endl;
    if (baseScore.find(probIDstr) != baseScore.end()){
      baseScore[probIDstr] = min(baseScore[probIDstr], steps);
    } else {
      baseScore[probIDstr] = steps;
    }
  }

  ofstream ofs;
  ofs.open(debugfile.c_str(), ios::out);
  for(auto &i: testcases){
    double value = ceil(size[i.probID]);
    value -= ceil(size[i.probID]*baseScore[i.probID]/i.step);
    i.value = -value;
    ofs<<i.print_debug()<<endl;
  }
  ofs.close();
}

//THIS IS BEST
void setupValue_prevbestscore(string debugfile){
  map<string, int> baseScore;
  string tmpline;
/*  ifstream ifs;
  ifs.open("bestscore.txt");
  while(getline(ifs,tmpline)){
    int prob,steps;
    char str[10];
    sscanf(tmpline.c_str(), "%d, %d, %s",&prob, &steps, str);
    string probIDstr = to_string(prob);
    while(probIDstr.length() < 3) probIDstr = "0" + probIDstr;
    probIDstr = "prob-"+probIDstr;
    //cout<<prob<<" "<<steps<<" "<<str<<" "<<probIDstr<<endl;
    if (baseScore.find(probIDstr) != baseScore.end()){
      baseScore[probIDstr] = min(baseScore[probIDstr], steps);
    } else {
      baseScore[probIDstr] = steps;
    }
  }
*/
  for(auto &i: testcases){
    if (baseScore.find(i.probID) != baseScore.end()){
      baseScore[i.probID] = min(baseScore[i.probID], i.step);
    } else {
      baseScore[i.probID] = i.step;
    }
  }


  //ofstream ofs;
  //ofs.open(debugfile.c_str(), ios::out);
  for(auto &i: testcases){
    double value = ceil(size[i.probID]*baseScore[i.probID]*0.82/i.step);
    i.value = value;
    cerr<<size[i.probID]<<" "<<value<<endl;
    //ofs<<i.print_debug()<<endl;
  }
  //ofs.close();

//TODO
/*
  vector<TestCase> tmpTestCase;
  int solutionPointer=0;

  for(int i=1;i<=PROBMAX;i++){
    string probIDstr = to_string(i);
    while(probIDstr.length() < 3) probIDstr = "0" + probIDstr;
    probIDstr = "prob-"+probIDstr;

    auto c = [](TestCase a,TestCase b){return a.value > b.value;};
    priority_queue<TestCase,vector<TestCase>, decltype(c) > pq(c);

    while(solutionPointer<(int)testcases.size()){
      if(testcases[solutionPointer].probID != probIDstr) break;
      while(!pq.empty() && pq.top().value < testcases[solutionPointer].value){
        pq.pop();
      }
      pq.push(testcases[solutionPointer]);
      solutionPointer++;
    }
    while(!pq.empty()){
      tmpTestCase.push_back(pq.top());
      pq.pop();
    }
  }

  testcases = tmpTestCase;
*/
  ofstream ofs;
  ofs.open(debugfile.c_str(), ios::out);
  for(auto &i: testcases){
    ofs<<i.print_debug()<<endl;
  }
  ofs.close();

//TODOTODO

}

void setupValue_disturbScore(string debugfile){
  map<string, int> baseScore;
  map<string, int> bestScore;
  string tmpline;
  ifstream ifs;
  ifs.open("bestscore.txt");
  while(getline(ifs,tmpline)){
    int prob,steps;
    char str[10];
    sscanf(tmpline.c_str(), "%d, %d, %s",&prob, &steps, str);
    string probIDstr = to_string(prob);
    while(probIDstr.length() < 3) probIDstr = "0" + probIDstr;
    probIDstr = "prob-"+probIDstr;
    //cout<<prob<<" "<<steps<<" "<<str<<" "<<probIDstr<<endl;
    if (bestScore.find(probIDstr) != bestScore.end()){
      bestScore[probIDstr] = min(bestScore[probIDstr], steps);
    } else {
      bestScore[probIDstr] = steps;
    }
  }
  for(auto i: testcases){
    if(i.cost == 0){
      if (baseScore.find(i.probID) != baseScore.end()){
        baseScore[i.probID] = min(i.step, baseScore[i.probID]);
      } else {
        baseScore[i.probID] = i.step;
      }
    }
  }
  ofstream ofs;
  ofs.open(debugfile.c_str(), ios::out);
  for(auto &i: testcases){
    double valuebase = ceil(size[i.probID]*baseScore[i.probID]/i.step)-ceil(size[i.probID]);
    double valuebest = ceil(size[i.probID]*bestScore[i.probID]/i.step)-ceil(size[i.probID]);
    //ceil(size[i.probID]*baseScore[i.probID]/i.step);
    i.value = valuebase+valuebest;
    ofs<<i.print_debug()<<endl;
  }
  ofs.close();
}

int solve(int cost, int func, string debugfile){
  void (* const valuefunc[])(string) = {
    setupValue_prevbestscore,
    setupValue_selfratio,
    setupValue_prevbestratio,
    setupValue_disturbScore
  };
  valuefunc[func](debugfile);
  //setupValue_prevbestratio();
  //setupValue_prevbestscore();
  ofstream ofs;
  for(int i=0;i<=cost;i++){
    dp[i][0] = -5000000.0;
    dp[i][1] = -5000000.0;
  }
  dp[0][0] = 0.0;

  int solutionPointer=0;
  int now=0;

  for(int i=1;i<=PROBMAX;i++){
    cerr<<i<<" ";
    string probIDstr = to_string(i);
    while(probIDstr.length() < 3) probIDstr = "0" + probIDstr;
    probIDstr = "prob-"+probIDstr;

    for(int j=0;j<=cost;j++)updated[i] = -1.0;
    ofs.open("./indextmp/"+probIDstr+".txt", ios::out);

    while(solutionPointer<(int)testcases.size()){
      if(testcases[solutionPointer].probID != probIDstr) break;
      for(int j=0;j<=cost;j++){
        int solutionCost = testcases[solutionPointer].cost;
        double solutionValue = testcases[solutionPointer].value;
        if((dp[j][now] > -5000000.0) && (j+solutionCost <= cost)){
          if(dp[j+solutionCost][now^1] < dp[j][now]+solutionValue){
            dp[j+solutionCost][now^1] = dp[j][now]+solutionValue;
            updated[j+solutionCost] = solutionPointer;
            //cout<<testcases[solutionPointer].print_debug()<<endl;
          }
        }
      }
      solutionPointer++;
    }
    for(int j=0;j<=cost;j++) ofs<<updated[j]<<" ";ofs<<endl;
    ofs.close();
    now^=1;
    for(int j=0;j<=cost;j++) dp[j][now^1] = -5000000.0;
  }
  return now;
}

void output(int cost,int now, string outputfile){

  double value = 0.0;
  int index = -1;
  for(int i=0;i<=cost;i++){
    if(value < (dp[i][now]+cost-i)){
      value = dp[i][now]+cost-i;
    //if(value < (dp[i][now])){
    //  value = dp[i][now];
      index = i;
    }
  }

  cerr<<index<<" "<<cost<<" "<<(cost-index)<<endl;
  ifstream ifs;
  ofstream ofs;
  double total =0.0;
  ofs.open(outputfile.c_str(), ios::out);
  for(int i=PROBMAX ;i>=1;i--){
    string probIDstr = to_string(i);
    while(probIDstr.length() < 3) probIDstr = "0" + probIDstr;
    probIDstr = "prob-"+probIDstr;
    ifs.open("./indextmp/"+probIDstr+".txt");
    string line; getline(ifs,line);
    ifs.close();
    stringstream ss{line};
    for(int j=0;j<=index;j++)ss>>prevstring[j];
    ofs<<testcases[prevstring[index]].origLine<<endl;
    total += testcases[prevstring[index]].value;
    index -= testcases[prevstring[index]].cost;
  }
  cerr<<total<<endl;
  ofs.close();
}

int main(int argc, char *argv[]){
  //Usage ./a.out ./test.txt ./problemsize.txt 1000000 0 ./value_debug.txt ./out.txt
  input(argv[1], argv[2]);
  int now = solve(stoi(argv[3]),stoi(argv[4]),argv[5]);
  output(stoi(argv[3]),now,argv[6]);
  return 0;
}
