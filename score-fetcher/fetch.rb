#!/usr/bin/env ruby

require "json"
require "uri"
require "net/http"

FIRST_VALID_BLOCK = 3

def fetch_raw_data(block)
  payload = {
    "jsonrpc" => "2.0",
    "method" => "getblockinfo",
    "id" => "1",
  }
  if block == -1
    payload["params"] = []
  else
    payload["params"] = [block]
  end

  uri = URI('http://localhost:8332')
  req = Net::HTTP::Post.new(uri, 'Content-Type' => 'application/json')
  req.body = payload.to_json
  res = Net::HTTP.start(uri.hostname, uri.port) do |http|
    http.request(req)
  end
  JSON.parse(res.read_body)["result"]
end

def get_current_block_num()
  res = fetch_raw_data(-1)
  res["block"]
end

def get_excluded(block)
  teams = JSON.parse(File.read('teams.json'))

  results = fetch_raw_data(block)
  excluded = results["excluded"][0]
  if excluded.nil?
    ""
  elsif teams.has_key?(excluded)
    teams[excluded]
  else
    excluded
  end
end

def fetch_task(block)
  results = fetch_raw_data(block)
  task = results["task"]
  task
end

def fetch_puzzle(block)
  results = fetch_raw_data(block)
  task = results["puzzle"]
  task
end

def fetch_balances(block)
  teams = JSON.parse(File.read('teams.json'))

  results = fetch_raw_data(block)
  raw_balances = results["balances"]
  balances = {}
  raw_balances.each{
    |id, amount|
    if teams.has_key?(id)
      balances[teams[id]] = amount
    else
      balances[id] = amount
    end
  }
  return balances
end

current_block = get_current_block_num

cached_tasks = JSON.parse(File.read('tasks.json'))
cached_puzzle = JSON.parse(File.read('puzzle.json'))
cached_balances = JSON.parse(File.read('balances.json'))
cached_excluded = JSON.parse(File.read('excluded.json'))

(FIRST_VALID_BLOCK..current_block).each{|block|
  if cached_tasks.length < (block - FIRST_VALID_BLOCK)
    cached_tasks.push(fetch_task(block))
  end
  if cached_puzzle.length < (block - FIRST_VALID_BLOCK)
    cached_puzzle.push(fetch_puzzle(block))
  end
  if cached_balances.length < (block - FIRST_VALID_BLOCK)
    cached_balances.push(fetch_balances(block))
  end
  if cached_excluded.length < (block - FIRST_VALID_BLOCK)
    cached_excluded.push(get_excluded(block))
  end
}
File.write('tasks.json', JSON.pretty_generate(cached_tasks))
File.write('puzzle.json', JSON.pretty_generate(cached_puzzle))
File.write('balances.json', JSON.pretty_generate(cached_balances))
File.write('excluded.json', JSON.pretty_generate(cached_excluded))

rankings = []
((FIRST_VALID_BLOCK+1)..(current_block-1)).each{|block|
  block -= 3
  before = cached_balances[block - 1]
  after = cached_balances[block]
  ranking = []
  after.each{|key, _|
    diff = after[key].to_i - before[key].to_i
    diff -= 2000 if cached_excluded[block] == key
    ranking.push({key => diff})
  }
  ranking.sort_by!{|v| -v.values[0] }
  rankings.push(ranking)
}
File.write('rankings.json', JSON.pretty_generate(rankings))
