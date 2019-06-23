#!/usr/bin/env ruby

require "json"

rankings = JSON.parse(File.read('rankings.json'))
team_list = []
rankings.each{|ranking|
  ranking.each{|v|
    team = v.keys[0]
    team_list.push(team) unless team_list.include?(team)
  }
}

total_team_num = team_list.length
current_ranking = rankings.last.map{|v| v.keys[0]}

relative_rankings = []

rankings.each{|ranking|
  top_score = ranking[0].values[0]
  relative_rank = {}
  ranking.each{|v|
    team = v.keys[0]
    score = v.values[0] / (top_score + 0.0)
    relative_rank[team] = score
  }

  relative_rankings.push(relative_rank)
}

relative_scores_by_team = []
current_ranking.each{|team|
  scores = []
  relative_rankings.each{|ranking|
    if ranking.key?(team)
      scores.push(ranking[team])
    else
      scores.push(0.0)
    end
  }
  relative_scores_by_team.push({team => scores})
}

puts JSON.pretty_generate(relative_scores_by_team)
