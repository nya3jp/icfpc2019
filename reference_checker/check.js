const fs = require('fs');
const process = require('process');

const lib = require('./lambda.js');


if (process.argv.length < 4) {
  console.error("Usage: nodejs validator.js [task] [solution] [purchase]");
  process.exit(1);
}

const task = fs.readFileSync(process.argv[2]);
const solution = fs.readFileSync(process.argv[3]);
const purchase = process.argv[4];

console.log(lib.check(task, solution, purchase));
