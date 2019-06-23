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
    return a.cost < b.cost;
  } else {
    return a.step < b.step;
  }
}

vector<TestCase> testcases;
map<string, double> size;
map<string, int> baseScore;
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
  }
}

void input(string solutionPath, string probsizePath){
  inputSolution(solutionPath);
  inputProbSize(probsizePath);
}

void setupValue_selfratio(){
  for(auto i: testcases){
    if(i.cost == 0){
      if (baseScore.find(i.probID) != baseScore.end()){
        baseScore[i.probID] = min(i.step, baseScore[i.probID]);
      } else {
        baseScore[i.probID] = i.step;
      }
    }
  }

  for(auto &i: testcases){
    double value = ceil(size[i.probID]);
    value -= ceil(size[i.probID]*i.step/baseScore[i.probID]);
    i.value = value;
    //cout<<i.print_debug()<<endl;
  }

}

void setupValue_prevbestratio(){

}

int solve(int cost){
  setupValue_selfratio();
  ofstream ofs;
  for(int i=0;i<=cost;i++){
    dp[i][0] = -1.0;
    dp[i][1] = -1.0;
  }
  dp[0][0] = 0.0;

  int solutionPointer=0;
  int now=0;

  for(int i=1;i<=PROBMAX;i++){
    cerr<<i<<endl;
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
        if((dp[j][now] > -1) && (j+solutionCost <= cost)){
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
    for(int j=0;j<=cost;j++) dp[j][now^1] = -1;
  }
  return now;
}

void output(int cost,int now){

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

  ifstream ifs;
  for(int i=PROBMAX ;i>=1;i--){
    string probIDstr = to_string(i);
    while(probIDstr.length() < 3) probIDstr = "0" + probIDstr;
    probIDstr = "prob-"+probIDstr;
    ifs.open("./indextmp/"+probIDstr+".txt");
    string line; getline(ifs,line);
    ifs.close();
    stringstream ss{line};
    for(int j=0;j<=index;j++)ss>>prevstring[j];
    cout<<testcases[prevstring[index]].origLine<<endl;
    index -= testcases[prevstring[index]].cost;
  }
}

int main(int argc, char *argv[]){
  //Usage ./a.out ./test.txt ./problemsize.txt 1000000
  input(argv[1], argv[2]);
  int now = solve(stoi(argv[3]));
  output(stoi(argv[3]),now);
  return 0;
}
