## ebd2html-osx

### Usage
Based on http://www.binword.com/blog/archives/000588.html

1. Use EBDump via Wine (NXWine or CrossOver) (list of files to extract: http://hp.vector.co.jp/authors/VA000022/ebd2html/ebd2html.html )
2. `export LC_ALL=ja_JP.SJIS; ./ebd2html`
3. `ruby epwing_conv.rb < COBUILD.html | ruby gaiji_rep.rb cobuild.lst > MyDictionary.xml`
4. `make`

Please note OSX 10.9+ must use `/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/bin/ruby`.

### I see strange characters in Terminal
Don't worry. Terminal just doesn't understand SJIS. Just use output html.

### License
Public Domain, or Creative Commons CC0.
