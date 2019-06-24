#!/usr/bin/env ruby

require "tempfile"
require "json"

FIRST_VALID_ROUND = 3

tasks = JSON.parse(File.read('tasks.json'))
tasks.each_with_index{|task, idx|
  Tempfile.create("task" + idx.to_s) do |f|
    File.write(f.path, task)
    found = false
    (1..300).each do |prob|
      path = "../problems/prob-%#03d.desc" % prob
      if system("diff #{f.path} #{path} >/dev/null")
        found = true
        puts "Found the task #{idx} is equals to #{prob}"
        return
      end
    end
    if !found
      puts "the task #{idx} is original"
    end
  end
}
