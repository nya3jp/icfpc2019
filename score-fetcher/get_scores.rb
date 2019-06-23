#!/usr/bin/env ruby

require "json"

TEAM = ARGV[0]
balances = JSON.parse(File.read('balances.json'))
round_scores = []
score = 0
balances.each{|round|
  new_score = 0
  new_score = round[TEAM] if round.has_key?(TEAM)
  diff = new_score - score
  score = new_score
  round_scores.push(diff)
}
puts TEAM + " got scores:"
puts JSON.pretty_generate(round_scores)
