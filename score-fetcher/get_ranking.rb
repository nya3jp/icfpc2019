#!/usr/bin/env ruby

require "json"

balances = JSON.parse(File.read('balances.json'))
rankings = JSON.parse(File.read('rankings.json'))
team_list = []
rankings.each{|ranking|
  ranking.each{|v|
    team = v.keys[0]
    team_list.push(team) unless team_list.include?(team)
  }
}

total_team_num = team_list.length
current_ranking = balances.last.sort_by {|v| -v[1]}

# File.write('current.json', JSON.pretty_generate(current_ranking))

relative_rankings = []

rankings.each{|ranking|
  top_score = ranking[0].values[0]
  relative_rank = {}
  ranking.each{|v|
    team = v.keys[0]
    score = v.values[0] / (top_score + 0.0)
    if score < 0.005
      relative_rank[team] = relative_rankings.last[team]
    else
      relative_rank[team] = score
    end
  }

  relative_rankings.push(relative_rank)
}

File.write('dump.json', JSON.pretty_generate(relative_rankings))

relative_scores_by_team = []
current_ranking.each{|team|
  scores = []
  relative_rankings.each{|ranking|
    if ranking.key?(team[0])
      scores.push(ranking[team[0]])
    else
      scores.push(0.0)
    end
  }
  relative_scores_by_team.push({team[0] => scores})
}

puts JSON.pretty_generate(relative_scores_by_team)
