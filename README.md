## epwing_conv
- Updated for OSX 10.9+ (Ruby 1.9+)
- Ruby 1.8 is not supported.

## Usage
Based on http://www.binword.com/blog/archives/000588.html

## Prerequisites
- Windows enviroment
  - Wine is alright. (ex) NXWine or CrossOver
- EBDump
  - http://ebstudio.info/home/EBDump.html
- Dictionary Development Kit
  - Make your Apple ID developer mode (free).
  - Access to https://developer.apple.com/download/
  - Select `See more downloads` below.
  - Search for `Auxiliary tools for Xcode` and download one of them
  - Open the archive and put the `Dictionary Development Kit` (our default is /Applications)

### Dumping text using EBDump
- Prepare Windows enviroment
  - Wine is alright. (ex) NXWine or CrossOver
- list of files to extract
  - http://hp.vector.co.jp/authors/VA000022/ebd2html/ebd2html.html

|Base file|Category|Output|
|:--|:--|:--|
|HONMON/START|[00]本文|honmon.txt|
|HONMON/START|[90]前方一致かなINDEX|fkindex.txt|
|HONMON/START|[04]前方一致かな見出し|fktitle.txt|
|HONMON/START|[91]前方一致表記INDEX|fhindex.txt|
|HONMON/START|[05]前方一致表記見出し|fhtitle.txt|
|HONMON/START|[92]前方一致英字INDEX|faindex.txt|
|HONMON/START|[08]前方一致英字見出し|fatitle.txt|
|GAIJI-file/START|[F1]外字(16×16ドット)|zgaiji.txt|
|GAIJI-file/START|[F2]外字(8×16ドット)|hgaiji.txt|

- `honmon.txt` is mandatory and one of `fkindex.txt/fktitle.txt`, `fhindex.txt/fhtitle.txt` or `faindex.txt/fatitle.txt` are also mandatory.
- 16x16 dot gaiji file is usually `GA16FULL` or `GAI16F00`.
- 8x16 dot gaiji file is usually `GA16HALF` or `GAI16H00`.

Note that:

- `出力ブロック数` (output blocks) should be the same as `blks`.
- `テキストダンプ` (text dump) type should be `記述子` (descriptor).

### Building HTML using ebd2html
- Put ebd2html and ebd2html.ini in the same directory as honmon.txt.
- Edit ebd2html.ini
  - ini comment should be descriptive but you can refer to http://hp.vector.co.jp/authors/VA000022/ebd2html/ebd2html.html for detailed help
- Perform conversion: `export LC_ALL=ja_JP.SJIS; ./ebd2html`
  - You will see strange characters in Terminal, but don't worry. Terminal just doesn't understand SJIS. The output should be fine.
- changing locale is required for `sort`.

### Building XML using epwing_conv
- (Let the HTML `COBUILD.html` and the GAIJI map `cobuild.lst`)
  - The GAIJI map is specific for each dictionaries, so you have to make one refering `Gaiji.xml`.
- `ruby epwing_conv.rb < COBUILD.html | ruby gaiji_rep.rb cobuild.lst > MyDictionary.xml`

### Building dictionary
- Put MyInfo.plist, Makefile and MyDictionary.xml to Dictionary Development Kit's project_template.
- Edit MyInfo.plist and Makefile's parameters properly.
- `make`

## License
Public Domain, or Creative Commons CC0.
