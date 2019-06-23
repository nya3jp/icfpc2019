#include<iostream>
#include <fstream>
#include<sstream>
#include<istream>
#include<string>
#include<vector>
#include<algorithm>
#include<map>
#include<set>

using namespace std;

#define PROBSIZE 300

string base = "../../problems/";

string makeProbName(int probID){
  string probIDstr = to_string(probID);
  while(probIDstr.length() < 3) probIDstr = "0" + probIDstr;
  string probName = "prob-" + probIDstr;
  return probName;
}

string getProblemPath(string probName){
  ifstream ifs;
  ifs.open(base+probName+".desc");
  string desc; ifs>>desc;
  ifs.close();
  return desc;
}

string getProblemState(string path){
  ifstream ifs;
  ifs.open(base+path);
  string problem; ifs>>problem;
  ifs.close();
  return problem;
}

string getFieldString(string problem){
  stringstream ss{problem};
  string fieldstring;
  getline(ss, fieldstring, '#');
  return fieldstring;
}

string calcOutputSize(int probID){

  string probName = makeProbName(probID);
  string probPath = getProblemPath(probName);
  string problem = getProblemState(probPath);
  string fieldstring = getFieldString(problem);

  stringstream fieldstream{fieldstring};
  string tmpfield;
  int maxX = 0;
  int maxY = 0;
  while(getline(fieldstream, tmpfield, ',')){
    int x; sscanf(tmpfield.c_str(),"(%d,", &x);
    getline(fieldstream, tmpfield, ',');
    int y; sscanf(tmpfield.c_str(),"%d)", &y);
    maxX = max(maxX, x);
    maxY = max(maxY, y);
  }
  return probName + " " + to_string(maxX) + " " + to_string(maxY);
}

int main(){
  ofstream ofs;
  ofs.open("./test.txt");
  for(int i=0;i<PROBSIZE;i++) ofs<<calcOutputSize(i+1)<<endl;
  ofs.close();
}
