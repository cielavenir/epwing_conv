#!/usr/bin/ruby
#coding:utf-8
#一括置換スクリプト
#Tats_y (http://www.binword.com/blog/)
#v0.01
#2008/02/11
Encoding.default_external='UTF-8'

list = File.readlines(ARGV.shift)
re = list.map{|i|
  temp = i.chomp.split("\t")
  [temp.first, temp.last]
}
while line = STDIN.gets
  re.each{|elem|
    line.gsub!(elem[0], elem[1])
  }
  print line
end
