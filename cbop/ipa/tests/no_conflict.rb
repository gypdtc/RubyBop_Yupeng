require 'set'
require 'pp'
@start = false

def no_conflict(lines)
  addr = lines.map { |line|
    line =~ /^child (?<child>\d+) Allocated\s(?<address>0x\h+)$/
    [$~[:child].to_i, $~[:address]] if $~
  }.select{|x|
    !x.nil?
  }
  counts = {}
  conflicts = {}
  addr.each{|x|
    child, address = x
    counts[address] ||= 0
    counts[address] += 1
    conflicts[address] ||= []
    conflicts[address] << child
  }
  if counts.values.none?{|v| v > 1}
    puts "success"
    exit(0)
  else
    conflicts.select{|addr, x| counts[addr] > 1}.each {|addr, allocators|
      puts "Addr: #{addr} Allocators: #{allocators.uniq.sort * ' '}"
    }
    puts "failure #{counts.values.select{|x| x > 1}.size} duplicates"
    exit(-1)
  end
end

if ARGV[0] then
  no_conflict File.readlines(ARGV[0])
else
  no_conflict STDIN.readlines
end
