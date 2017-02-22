#!/usr/bin/ruby
#coding:utf-8
#ebd2htmlの出力結果を
#Mac OS X v10.5 "Leopard"の辞書アプリケーション（Dictionary.app）用のXMLに変換する
#by Tats_y (http://www.binword.com/blog/)
#v0.01
#2008/02/11

require 'nkf' # to convert Zenkaku-alphabet to Hankaku

#select appropriate one
STDIN.set_encoding("Windows-31J","UTF-8") #SJIS
#STDIN.set_encoding("ISO-2022-JP","UTF-8") #JIS

id = ""
title = ""
key = Array.new
body = ""

print '<?xml version="1.0" encoding="UTF-8"?>' + "\n"
print '<d:dictionary xmlns="http://www.w3.org/1999/xhtml" xmlns:d="http://www.apple.com/DTDs/DictionaryService-1.0.rng">' + "\n"

while line = gets
  next if line.strip.empty?  #空行を読み飛ばす
  line1 = line  #ISO-2022-JPからUTF-8へ変換
  if line1 =~ /^<dt id=/ then
    id = line1.slice(/<dt id="([A-Z\d]+)">/, 1)  #idを取得
    title = line1.slice(/<dt id=".+">(.+)<\/dt>/, 1)  #項目名を取得
  end
  if line1 =~ /^<key title/ then
    key.push(NKF.nkf('-wZ1', line1.slice(/<key title=".+" type=".+">(.+)<\/key>/, 1).to_s))  #項目のキーを配列に格納
  end
  if line1 =~ /^<dd>/ then
    body = "\t<p>\n"
    title2 = iconv.iconv(gets).gsub(/<br>|<nobr>|<\/nobr>/,"").chomp  #本文の1行目はタイトルに使う
    while line2 = iconv.iconv(gets)
      if line2 =~ /^<\/p><\/dd>|^<\/dl>/ then   #項目の最終行は</dd>もしくは</dl>で判定する（ebd2htmlの出力では最後の項目が</dd>で閉じられていないため）
        body = body.gsub(/&#x1f09;&#x00;&#x02;|<nobr>|<\/nobr>/,"")      #各行に入っている余計な文字列を削除
        body = body.gsub(/&#x1f09;&#x00;&#x03;/,"&nbsp;")      #各行に入っている余計な文字列を削除
        body = body.gsub(/<br>/, "<br/>")
        body = body + "\t</p>"  #末尾を</p>で閉じる
        body = body.gsub(/<a href="#([0-9A-Z]+)">/) { "<a href=\"x-dictionary:r:" + $1 + "\">"}  #リンクの形式を変換
        title3 = (title + title2.gsub(/<sub>|<\/sub>/, "")).strip
        print '<d:entry id="' + id + '" d:title="' + title3 + "\">\n"
        print "\t<d:index d:value=\"" + title + '" d:title="' + title3 + "\"/>\n"
        key.each { |elem|
          print "\t<d:index d:value=\"" + elem + "\" d:title=\"" + title3 +"\"/>\n"
        }
        print "\t<h1>" + (title + title2).strip + "</h1>\n"
        print body + "\n"
        print "</d:entry>\n"
        id = ""
        title = ""
        title2 = ""
        key = []
        body = ""
        break
      end
      body = body + "\t" + line2
    end
  end
end

print "</d:dictionary>\n"

