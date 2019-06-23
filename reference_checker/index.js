const lib = require('./lambda.js');

exports.check = async function(req, res) {
  const { task, solution, purchase } = req.body;
  if (!task || !solution) {
    res.status(400).send('Required parameters not set');
    return
  }

  const output = lib.check(task, solution, purchase);
  res.status(200).send(output);
};
