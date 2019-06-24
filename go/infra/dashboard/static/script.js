google.charts.load('current', {packages: ['corechart', 'line']});
google.charts.setOnLoadCallback(draw);

data_url = 'https://storage.googleapis.com/sound-type-system/rankings/ranking_data.json'
last = 30;

function filterChange(filter) {
    if (filter == "") {
      draw();
    } else {
      drawWithFilter(filter);
    }
}

function lastChange(new_last) {
  last = new_last;
  draw();
}

function filtering(filter, raw_data) {
  filtered = [];
  reg = new RegExp(filter, "i");
  raw_data.forEach(function(elem) {
    for (team in elem) {
      if (reg.test(team)) {
        filtered.push(elem);
      }
    }
  });
  return filtered;
}

function drawWithFilter(filter) {
  var xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var raw_data = JSON.parse(this.responseText);
      filtered = filtering(filter, raw_data);
      drawRanking(filtered);
    }
  }
  xmlhttp.open("GET", data_url, true);
  xmlhttp.send();
}

function draw() {
  var xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var raw_data = JSON.parse(this.responseText);
      drawRanking(raw_data);
    }
  };
  xmlhttp.open("GET", data_url, true);
  xmlhttp.send();
}

function drawRanking(raw_data) {
  var data = new google.visualization.DataTable();
  rounds = 0;
  data.addColumn('number', 'Round');
  raw_data = raw_data.slice(0, 9);
  raw_data.forEach(function(elem) {
    for (team in elem) {
      data.addColumn('number', team);
      rounds = elem[team].length
    }
  });

   rows = [];
   for (var i = 0; i < last; ++i) {
      r = rounds - last + i;
      rows.push([r]);
      raw_data.forEach(function(elem) {
        for (team in elem) {
          rows[i].push(elem[team][i]);
        }
      });
   }
   data.addRows(rows);

   var options = {
     explorer: {},
     hAxis: {
       title: 'Round'
     },
     vAxis: {
       title: 'Rank'
     }
   };

   var chart = new google.visualization.LineChart(document.getElementById('chart_div'));

   chart.draw(data, options);
}
