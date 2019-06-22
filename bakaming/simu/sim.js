var canvas = document.getElementById("canvas");
const MAX_X = 500;
const MAX_Y = 500;
const CANVAS_MAX_X = 500;
const CANVAS_MAX_Y = 500;
let wall = Array.from(new Array(MAX_X), () => new Array(MAX_Y).fill(0));
let map = Array.from(new Array(MAX_X), () => new Array(MAX_Y).fill(0));
let sweep = Array.from(new Array(MAX_X), () => new Array(MAX_Y).fill(0));
let mapMaxX = 0;
let mapMaxY = 0;
let lobot;
let manipList;
let flags;

function showinfo(){
  let info = document.getElementById("info");
  info.value = "";
  info.value += "x:" + String(lobot[0]) + " y:" + String(lobot[1])+"\n";
  info.value += "maniplist:" + JSON.stringify(manipList) +"\n";
  info.value += "maniplist:" + JSON.stringify(flags) +"\n";
}

function addWall(wallList){
  for(i in wallList){
    let jstart = Math.min(wallList[i][0][0],wallList[i][1][0]);
    let jend = Math.max(wallList[i][0][0],wallList[i][1][0]);
    for(let j = jstart; j <= jend; j++){
      let kstart = Math.min(wallList[i][0][1],wallList[i][1][1]);
      let kend = Math.max(wallList[i][0][1],wallList[i][1][1]);
      for(let k = kstart; k <= kend; k++){
        if(j === jstart){
          if(k === kstart){
            continue;
          }
        }
        if(kstart === kend){
          wall[j][k] += 1;
        } else {
          wall[j][k] += 2;
        }
      }
    }
  }
  return;
}

function makeWallList(pointList){
  let ret = [];
  pointList = pointList.split(',');
  for (i in pointList) {
    pointList[i] = pointList[i].replace('(','');
    pointList[i] = pointList[i].replace(')','');
  }
  pointList.push(pointList[0]);
  pointList.push(pointList[1]);

  for(i in pointList){
    if(i <= 1) continue;
    if(i%2 === 0) continue;
    ret.push([[Number(pointList[i-3]),Number(pointList[i-2])], [Number(pointList[i-1]),Number(pointList[i])]]);
  }
  return ret;
}

function makeMap(){
  for(let i = 0; i<MAX_X-1; i++){
    for(let j = MAX_Y-2; j>=0 ; j--){
      map[i][j] = map[i][j+1];
      if(wall[i+1][j+1] % 2 === 1){
        map[i][j] = map[i][j] ^ 1;
      }
      if(map[i][j] === 1){
        mapMaxX = Math.max(i,mapMaxX);
        mapMaxY = Math.max(j,mapMaxY);
      }
    }
  }
  return;
}

function setStart(start){
  start = start.split(',');
  lobot = [0,0];
  lobot[0] = Number(start[0].replace('(',''));
  lobot[1] = Number(start[1].replace(')',''));
  manipList = [];
  manipList.push([1,1]);
  manipList.push([1,0]);
  manipList.push([1,-1]);
  flags = {};
  flags["L"] = 0;
  flags["F"] = 0;
  flags["B"] = 0;
  flags["Lcount"] = 0;
  flags["Fcount"] = 0;
  flags["Bcount"] = 0;
  flags["R"] = [];
  flags["Rcount"] = 0;
  return;
}

var mapop = {
  "B" : 3,
  "F" : 4,
  "L" : 5,
  "X" : 6,
  "R" : 7,
};

function setBoost(boost){
  boost = boost.split(';');
  for(i in boost){
    let num = mapop[boost[i][0]];
    boost[i] = boost[i].slice(1);
    let point = boost[i].split(',');
    point[0] = Number(point[0].replace('(',''));
    point[1] = Number(point[1].replace(')',''));
    map[point[0]][point[1]] = num;
  }
  return;
}

function render(){
  canvas.getContext("2d").clearRect(0, 0, 1000, 1000);
  let maxMapLen = Math.max(mapMaxX,mapMaxY)+1;
  let ctx = canvas.getContext('2d');
  let mapXunit = CANVAS_MAX_X/maxMapLen;
  let mapYunit = CANVAS_MAX_Y/maxMapLen;
  ctx.fillStyle = 'rgb(0, 0, 0)';
  for(let i = 0;i <= maxMapLen; i++){
    for(let j = 0;j <= maxMapLen; j++){
      switch (map[i][j]) {
        case 0: //wall
          ctx.fillStyle = 'rgb(0, 0, 0)';
          break;
        case 3: //B
          ctx.fillStyle = 'rgb(227, 199, 0)';
          break;
        case 4: //F
          ctx.fillStyle = 'rgb(127, 68, 40)';
          break;
        case 5: //L
          ctx.fillStyle = 'rgb(56, 151, 35)';
          break;
        case 6: //X
          ctx.fillStyle = 'rgb(0, 0, 255)';
          break;
        case 7: //R
          ctx.fillStyle = 'rgb(136, 72, 172)';
          break;
        case 8: //R set
          ctx.fillStyle = 'rgb(68, 36, 77)';
          break;
        default:
      }
      if(map[i][j] != 1){
        ctx.fillRect(i*mapXunit,CANVAS_MAX_Y-j*mapYunit-mapYunit, mapXunit, mapYunit );
      } else {
        if(sweep[i][j] === 1){
          ctx.fillStyle = 'rgb(0, 255, 0)';
          ctx.fillRect(i*mapXunit,CANVAS_MAX_Y-j*mapYunit-mapYunit, mapXunit, mapYunit );
        }
      }
    }
  }

  ctx.fillStyle = 'rgb(255, 0, 0)';
  ctx.fillRect(lobot[0]*mapXunit,CANVAS_MAX_Y-lobot[1]*mapYunit-mapYunit, mapXunit, mapYunit );
  for(i in manipList){
    if(visible(lobot[0],lobot[1],lobot[0]+manipList[i][0],lobot[1]+manipList[i][1])){
      ctx.fillRect((lobot[0]+manipList[i][0])*mapXunit,CANVAS_MAX_Y-(lobot[1]+manipList[i][1])*mapYunit-mapYunit, mapXunit, mapYunit );
    }
  }

}

var ansStr = "";

var moovKeys = {
  37: "A",
  39: "D",
  40: "S",
  38: "W",
  8: "BS",
  69: "E",
  81: "Q",
  66: "B",
  70: "F",
  76: "L",
  82: "R",
  84: "T",
};

function backspace_str(str){
  if(str.slice(-1) === ')'){
    return str.slice(0,str.lastIndexOf('(')-1);
  } else {
    return str.slice(0,-1);
  }
}

function callIndexBox(move){
  let index = window.prompt("input index", "");
  if(index.length > 0){
    ansStr += move + "("+index+")";
  } else{
    window.alert('cansel');
  }
}

function ansStrUpdate(move){
  if(move === "BS"){
    ansStr = backspace_str(ansStr);
    return;
  }
  switch (move) {
    case "B":
      callIndexBox(move);
      break;
    case "T":
      callIndexBox(move);
      break;
    default:
      ansStr += move;
  }
  return;
}

var lobotDir = {
  "A" : [-1,0],
  "D" : [1,0],
  "S" : [0,-1],
  "W" : [0,1],
};

function updateflags(){
  if(flags["F"] > 0) flags["F"]--;
  if(flags["L"] > 0) flags["L"]--;
}

function movelobot(move){
  let newX = lobot[0] + lobotDir[move][0];
  let newY = lobot[1] + lobotDir[move][1];

  if((newX < 0) || (mapMaxX < newX)){
    return;
  }
  if((newY < 0) || (mapMaxY < newY)){
    return;
  }

  switch (map[newX][newY]) {
    case 0:
      if(flags["L"] > 0){
        map[newX][newY] = 1;
      } else {
        return;
      }
    case 3:
      flags["Bcount"]++;
      map[newX][newY] = 1;
      break;
    case 4:
      flags["Fcount"]++;
      map[newX][newY] = 1;
      break;
    case 5:
      flags["Lcount"]++;
      map[newX][newY] = 1;
      break;
    case 7:
      flags["Rcount"]++;
      map[newX][newY] = 1;
      break;
    default:
  }
  lobot[0] = newX;
  lobot[1] = newY;
  sweep[lobot[0]][lobot[1]] = 1;
  for(i in manipList){
    if(visible(lobot[0],lobot[1],lobot[0]+manipList[i][0],lobot[1]+manipList[i][1])){
      sweep[lobot[0]+manipList[i][0]][lobot[1]+manipList[i][1]] = 1;
    }
  }
}

var pointdir = [
  [0.5,0.5],
  [0.5,-0.5],
  [-0.5,0.5],
  [-0.5,-0.5],
];

function visible(x1,y1,x2,y2){
  let maxx = Math.max(x1,x2);
  let minx = Math.min(x1,x2);
  let maxy = Math.max(y1,y2);
  let miny = Math.min(y1,y2);
  let dirx = x1-x2;
  let diry = y1-y2;
  if((x2 < 0) || (mapMaxX < x2)){
    return false;
  }
  if((y2 < 0) || (mapMaxY < y2)){
    return false;
  }
  if(map[x2][y2] === 0) return false;
  for(let i = minx; i<= maxx;i++){
    for(let j = miny; j<= maxy; j++){
      if((x1 === i) && (y1 === j)) continue;
      if((x2 === i) && (y2 === j)) continue;
      if(map[i][j] === 0){
        let crossdir = [];
        for(let k = 0;k<4;k++){
          let nowdirx = x1-i-pointdir[k][0];
          let nowdiry = y1-j-pointdir[k][1];
          crossdir[k] = Math.sign(dirx*nowdiry-diry*nowdirx);
        }
        let plus = 0;
        for(let k=0;k<4;k++){
          if(crossdir[k]>=0){
            plus++;
          }
        }
        let minus = 0;
        for(let k=0;k<4;k++){
          if(crossdir[k]<=0){
            minus++;
          }
        }
        if(plus === 4) continue;
        if(minus === 4) continue;
        return false;
      }
    }
  }
  return true;
}

function moveA(){
  movelobot("A");
  if(flags["F"] > 0){
    movelobot("A");
  }
  return;
}

function moveD(){
  movelobot("D");
  if(flags["F"] > 0){
    movelobot("D");
  }
  return;
}

function moveS(){
  movelobot("S");
  if(flags["F"] > 0){
    movelobot("S");
  }
  return;
}

function moveW(){
  movelobot("W");
  if(flags["F"] > 0){
    movelobot("W");
  }
  return;
}

function moveE(){
  for(i in manipList){
    manipList[i] = [manipList[i][1],-manipList[i][0]];
  }
  return;
}

function moveQ(){
  for(i in manipList){
    manipList[i] = [-manipList[i][1],manipList[i][0]];
  }
  return;
}

function moveB(){
  if(flags["Bcount"] === 0) return;
  let str = ansStr.slice(ansStr.lastIndexOf('('));
  str = str.split(',');
  str[0] = Number(str[0].replace('(',''));
  str[1] = Number(str[1].replace(')',''));
  manipList.push([str[0],str[1]]);
  flags["Bcount"]--;
  return;
}

function moveF(){
  if(flags["Fcount"] > 0){
    flags["F"] = 50;
    flags["Fcount"]--;
  }
  return;
}

function moveL(){
  if(flags["Lcount"] > 0){
    flags["L"] = 30;
    flags["Lcount"]--;
  }
  return;
}

function moveR(){
  if(flags["Rcount"] === 0) return;
  flags["R"].push([lobot[0],lobot[1]]);
  map[lobot[0]][lobot[1]] = 8;
  flags["Rcount"]--;
  return;
}

function moveBS(){

}

function moveT(){
  let str = ansStr.slice(ansStr.lastIndexOf('('));
  str = str.split(',');
  str[0] = Number(str[0].replace('(',''));
  str[1] = Number(str[1].replace(')',''));
  for(i in flags["R"]){
    if((str[0]===flags["R"][i][0]) && (str[1]===flags["R"][i][1])){
      lobot[0] = flags["R"][i][0];
      lobot[1] = flags["R"][i][1];
      return;
    }
  }
  return;
}

function moveBS(){
  let prevstr = ansStr;
  init();
  for(let i = 0;i<prevstr.length;i++){
    let move = prevstr[i];
    if((move=== 'B') || (move === 'T')){
      while(true){
        ansStr += prevstr[i];
        i++;
        if(prevstr[i] === ')') break;
      }
    } else {
      ansStr += move;
    }
    mapStatusUpdate(move);
  }
}

function mapStatusUpdate(move){
  switch (move) {
    case "A":
      moveA();
      break;
    case "D":
      moveD();
      break;
    case "S":
      moveS();
      break;
    case "W":
      moveW();
      break;
    case "E":
      moveE();
      break;
    case "Q":
      moveQ();
      break;
    case "B":
      moveB();
      break;
    case "F":
      moveF();
      break;
    case "L":
      moveL();
      break;
    case "R":
      moveR();
      break;
    case "T":
      moveT();
      break;
    case "BS":
      moveBS();
      break;
    default:
  }
  updateflags();
}

function valueinit(){
  wall = Array.from(new Array(MAX_X), () => new Array(MAX_Y).fill(0));
  map = Array.from(new Array(MAX_X), () => new Array(MAX_Y).fill(0));
  sweep = Array.from(new Array(MAX_X), () => new Array(MAX_Y).fill(0));
  mapMaxX = 0;
  mapMaxY = 0;
  return;
}

function init(){

  valueinit();

  let inputStr = document.getElementById("input").value;

  inputStr = inputStr.split('#');

  let globalwall = makeWallList(inputStr[0]);
  addWall(globalwall);

  let obstList = inputStr[2].split(';');
  for(i in obstList){
    let localwall = makeWallList(obstList[i]);
    addWall(localwall);
  }

  makeMap();

  let startPoint = inputStr[1];
  setStart(startPoint);

  let boostPoint = inputStr[3];
  if(boostPoint.length > 0){
    setBoost(boostPoint);
  }
  document.body.onkeydown = function(e){
    if(moovKeys[e.keyCode]){
      let move = moovKeys[e.keyCode];
      ansStrUpdate(move);
      mapStatusUpdate(move);
      render();
      showinfo();
      document.getElementById("output").value = ansStr;
    }
  };

  sweep[lobot[0]][lobot[1]] = 1;
  document.getElementById("output").value = "";
  ansStr = "";

  render();
  showinfo();

}

let urlParams = new URLSearchParams(window.location.search);
let problem = urlParams.get('problem');

if(problem != null){
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "https://storage.googleapis.com/sound-type-system/problems/"+problem+".desc", true);
  xhr.onload = function (e) {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        document.getElementById("input").value = xhr.responseText;
        init();
      } else {
        console.error(xhr.statusText);
      }
    }
  };
  xhr.onerror = function (e) {
    console.error(xhr.statusText);
  };
  xhr.send(null);
}
