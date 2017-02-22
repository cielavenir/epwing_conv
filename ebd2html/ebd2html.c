/*
 * ebd2html - EBDumpの出力からEBStudioへの入力HTMLファイルを再構成する試み
 *	Written by Junn Ohta (ohta@sdg.mdd.ricoh.co.jp), Public Domain.
 */

char	*progname = "ebd2html";
char	*version  = "experimental-0.05a for OSX";
char	*date     = "2014/03/14";

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <io.h>
//#include <dir.h>

typedef	unsigned char	byte;
typedef	unsigned int	word;
typedef	unsigned long	dword;

#ifndef O_BINARY
#define	O_BINARY	0
#endif

#define	OK		0
#define	ERR		(-1)

#define	TRUE		1
#define	FALSE		0

/* -------------------- ユーティリティー関数 -------------------- */ 

#define	LOG_FILE	"ebd2html.log"	/* ログファイル			*/

#define	MAX_WORD	256		/* 単語の最大長			*/

/*
 * 2バイト記号→1バイト記号変換表(0x8140～0x8197)
 */
int zen2han[] = {
    ' ', ERR, ERR, ',', '.', ERR, ':', ';',
    '?', '!', ERR, ERR, ERR, ERR, ERR, '^',
    '~', '_', ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, '/','\\',
    '~', '|', '|', ERR, ERR,'\'','\'', '"',
    '"', '(', ')', '[', ']', '[', ']', '{',
    '}', ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, '+', '-', ERR, ERR, ERR,
    ERR, '=', ERR, '<', '>', ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR,'\\',
    '$', ERR, ERR, '%', '#', '&', '*', '@'
};

int	hexval(byte *);
byte	*skipsp(byte *);
byte	*skipch(byte *, int);
byte	*skipstr(byte *, byte *);
byte	*endstr(byte *);
byte	*addstr(byte *, byte *);
int	tohan(byte *);
byte	*getuptos(byte *, byte *, byte *);
byte	*getupto(byte *, byte *, byte);
int	iskanastr(byte *);
void	write_log(char *, ...);
void	message(char *, ...);

/*
 * 2桁の16進数が示す値を返す
 */
int
hexval(byte *p)
{
    int i, n;

    n = 0;
    for (i = 0; i < 2; i++) {
	n <<= 4;
	if (*p >= '0' && *p <= '9')
	    n += *p - '0';
	else if (*p >= 'a' && *p <= 'f')
	    n += *p - 'a' + 10;
	else if (*p >= 'A' && *p <= 'F')
	    n += *p - 'A' + 10;
	else
	    n = 0;
	p++;
    }
    return n;
}

/*
 * 空白を読み飛ばし、次の位置を返す
 */
byte *
skipsp(byte *str)
{
    while (*str == ' ' || *str == '\t')
	str++;
    return str;
}

/*
 * 指定文字まで読み飛ばし、その次の位置を返す
 */
byte *
skipch(byte *str, int ch)
{
    while (*str && *str != ch) {
	if (*str & 0x80)
	    str++;
	str++;
    }
    if (*str == ch)
	str++;
    return str;
}

/*
 * 指定文字列まで読み飛ばし、その次の位置を返す
 * (文字列がなければ末尾の位置を返す)
 */
byte *
skipstr(byte *str, byte *key)
{
    byte *p;

    p = strstr(str, key);
    if (p)
	return p + strlen(key);
    return endstr(str);
}

/*
 * 文字列の末尾の位置を返す
 */
byte *
endstr(byte *str)
{
    while (*str)
	str++;
    return str;
}

/*
 * バッファに文字列を追加し、その末尾を返す(NULで終了はしない)
 */
byte *
addstr(byte *dst, byte *str)
{
    while (*str)
	*dst++ = *str++;
    return dst;
}

/*
 * 2バイト英数字を1バイト英数字に変換する
 */
int
tohan(byte *str)
{
    int high, low;

    high = str[0];
    low = str[1];
    if (high == 0x82) {
	if (low >= 0x4f && low <= 0x58)
	    return '0' + low - 0x4f;
	if (low >= 0x60 && low <= 0x79)
	    return 'A' + low - 0x60;
	if (low >= 0x81 && low <= 0x9a)
	    return 'a' + low - 0x81;
	return ERR;
    }
    if (high == 0x81) {
	if (low >= 0x40 && low <= 0x97)
	    return zen2han[low - 0x40];
	return ERR;
    }
    return ERR;
}

/*
 * 区切り文字セットまでの単語を切り出し、区切り文字の位置を返す
 */
byte *
getuptos(byte *buf, byte *word, byte *stops)
{
    byte *p, *q;

    p = buf;
    q = word;
    while (*p && *p != '\r' && *p != '\n') {
	if (strchr(stops, *p))
	    break;
	if (*p & 0x80)
	    *q++ = *p++;
	*q++ = *p++;
    }
    *q = '\0';
    return p;
}

/*
 * 区切り文字までの単語を切り出し、区切り文字の位置を返す
 */
byte *
getupto(byte *buf, byte *word, byte stop)
{
    byte *p, *q;

    p = buf;
    q = word;
    while (*p && *p != stop && *p != '\r' && *p != '\n') {
	if (*p & 0x80)
	    *q++ = *p++;
	*q++ = *p++;
    }
    *q = '\0';
    return p;
}

/*
 * 文字列がひらがな/カタカナ/長音のみで構成されているか?
 */
int
iskanastr(byte *str)
{
    while (*str) {
	if (str[0] == 0x81 && str[1] == 0x5b ||
	    str[0] == 0x82 && str[1] >= 0x9f ||
	    str[0] == 0x83 && str[1] <= 0x96) {
	    str += 2;
	    continue;
	}
	return FALSE;
    }
    return TRUE;
}

/*
 * メッセージをログファイルに書く
 */
void
write_log(char *fmt, ...)
{
    char buf[BUFSIZ];
    FILE *fp;
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    if ((fp = fopen(LOG_FILE, "a")) == NULL)
	return;
    fputs(buf, fp);
    fclose(fp);
}

/*
 * メッセージを表示し、ログファイルにも書く
 */
void
message(char *fmt, ...)
{
    char buf[BUFSIZ];
    FILE *fp;
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    fputs(buf, stdout);
    fflush(stdout);
    if ((fp = fopen(LOG_FILE, "a")) == NULL)
	return;
    fputs(buf, fp);
    fclose(fp);
}

/* -------------------- メイン -------------------- */ 

#define	MAX_PATH	1024		/* パス名の最大長		*/
#define	MAX_DLINE	1024		/* データファイルの最大行長	*/
#define	MAX_HLINE	256		/* HTMLファイルのおよその最大長	*/
#define	MAX_BUFF	65535		/* 本文データ行の最大長(64KB)	*/

#define	FKINDEX_FILE	"fkindex.txt"	/* かなインデックスダンプデータ	*/
#define	FKINDEX_WORK	"fkindex.tmp"	/* かなインデックス一時ファイル	*/
#define	FKINDEX_DATA	"fkindex.dat"	/* ソート済みかなインデックス	*/
#define	FKTITLE_FILE	"fktitle.txt"	/* かな見出しダンプデータ	*/
#define	FKTITLE_DATA	"fktitle.dat"	/* かな見出し作業バイナリデータ	*/
#define	FHINDEX_FILE	"fhindex.txt"	/* 表記インデックスダンプデータ	*/
#define	FHINDEX_WORK	"fhindex.tmp"	/* 表記インデックス一時ファイル	*/
#define	FHINDEX_DATA	"fhindex.dat"	/* ソート済み表記インデックス	*/
#define	FHTITLE_FILE	"fhtitle.txt"	/* 表記見出しダンプデータ	*/
#define	FHTITLE_DATA	"fhtitle.dat"	/* 表記見出し作業バイナリデータ	*/
#define	FAINDEX_FILE	"faindex.txt"	/* 英字インデックスダンプデータ	*/
#define	FAINDEX_WORK	"faindex.tmp"	/* 英字インデックス一時ファイル	*/
#define	FAINDEX_DATA	"faindex.dat"	/* ソート済み英字インデックス	*/
#define	FATITLE_FILE	"fatitle.txt"	/* 英字見出しダンプデータ	*/
#define	FATITLE_DATA	"fatitle.dat"	/* 英字見出し作業バイナリデータ	*/
#define	ZGAIJI_FILE	"zgaiji.txt"	/* 全角16ドット外字ダンプデータ	*/
#define	HGAIJI_FILE	"hgaiji.txt"	/* 半角16ドット外字ダンプデータ	*/
#define	GFONT_FILE	"Gaiji.xml"	/* EBStudio用外字マップファイル	*/
#define	GMAP_FILE	"GaijiMap.xml"	/* EBStudio用外字フォント	*/
#define	HONMON_FILE	"honmon.txt"	/* 本文ダンプデータ		*/
#define	INI_FILE	"ebd2html.ini"	/* ebd2html設定ファイル		*/

typedef struct index_t {		/* インデックス/見出しデータ	*/
    dword	dblk;			/* 本文ブロック番号		*/
    word	dpos;			/* 本文ブロック内位置		*/
    dword	tblk;			/* 見出しブロック番号		*/
    word	tpos;			/* 見出しブロック内位置		*/
    byte	str[MAX_WORD];		/* インデックス文字列		*/
    byte	title[MAX_WORD];	/* 見出し文字列			*/
} INDEX;

typedef struct honmon_t {		/* 本文データ			*/
    dword	dblk;			/* 本文ブロック番号		*/
    word	dpos;			/* 本文ブロック内位置		*/
    byte	buf[MAX_BUFF];		/* 本文テキスト			*/
} HONMON;

byte	*base_path = "";		/* EBStudio基準ディレクトリ	*/
byte	*out_path = "";			/* EBStudio出力ディレクトリ	*/
byte	*sort_cmd = "";			/* ソートコマンドのパス		*/
int	auto_kana = 0;			/* 表記INDEXからかなINDEXを生成	*/
int	eb_type = 0;			/* 0:EPWING, 1:電子ブック	*/
byte	*book_title = "";		/* 書籍タイトル			*/
byte	*book_type = "";		/* 書籍種別			*/
byte	*book_dir = "";			/* 書籍ディレクトリ名		*/
byte	*html_file = "";		/* 生成されるHTMLファイル名	*/
byte	*ebs_file = "";			/* 生成されるEBSファイル名	*/
time_t	start_time;			/* 開始時刻			*/
time_t	stop_time;			/* 終了時刻			*/
int	zg_start_unicode;		/* 全角外字開始Unicodeコード	*/
int	hg_start_unicode;		/* 半角外字開始Unicodeコード	*/
int	zg_start_ebhigh;		/* 全角外字開始ebcode上位byte	*/
int	hg_start_ebhigh;		/* 半角外字開始ebcode上位byte	*/
int	zg_orig_ebhigh;		/* 元データの全角外字開始コード上位byte	*/
int	zg_orig_eblow;		/* 元データの全角外字開始コード下位byte	*/
int	hg_orig_ebhigh;		/* 元データの半角外字開始コード上位byte	*/
int	hg_orig_eblow;		/* 元データの半角外字開始コード下位byte	*/
dword	fktitle_start_block;		/* かな見出し開始ブロック番号	*/
dword	fhtitle_start_block;		/* 表記見出し開始ブロック番号	*/
dword	fatitle_start_block;		/* 英字見出し開始ブロック番号	*/
int	gen_kana;			/* かなインデックスを作る	*/
int	gen_hyoki;			/* 表記インデックスを作る	*/
int	gen_alpha;			/* 英字インデックスを作る	*/
int	have_auto_kana;			/* auto_kana検索語がある	*/

int	generate_gaiji_file(void);
byte	*gstr(byte *, int);
byte	*conv_title(byte *, byte *);
int	convert_index_data(FILE *, FILE *);
int	convert_title_data(FILE *, int);
int	generate_work_file(void);
FILE	*html_newfile(void);
int	html_close(FILE *);
INDEX	*read_index_data(FILE *, int, dword, INDEX *);
HONMON	*read_honmon_data(FILE *, HONMON *);
int	compare_position(INDEX *, HONMON *);
byte	*conv_honmon(byte *, byte *);
byte	*skipindent(byte *, int *);
byte	*indentstr(int);
int	generate_html_file(void);
int	generate_ebs_file(void);
int	parse_ini_file(void);
int	work_directory(void);
int	set_sort_command(void);
int	init(char *);
void	term(int);
int	main(int, char **);

/*
 * 外字ダンプファイルからGaiji.xmlとGaijiMap.xmlを作る
 */
int
generate_gaiji_file(void)
{
    int first, unicode, ebhigh, eblow;
    byte *p, buf[MAX_DLINE];
    FILE *fp, *ffp, *mfp;

    sprintf(buf, "%s%s", base_path, GMAP_FILE);
    if ((mfp = fopen(buf, "w")) == NULL) {
	message("外字マップファイル %s が新規作成できません\n", buf);
	return ERR;
    }
    sprintf(buf, "%s%s", base_path, GFONT_FILE);
    if ((ffp = fopen(buf, "w")) == NULL) {
	message("外字フォントファイル %s が新規作成できません\n", buf);
	return ERR;
    }
    fprintf(mfp, "<?xml version=\"1.0\" encoding=\"Shift_JIS\"?>\n");
    fprintf(mfp, "<gaijiSet>\n");
    fprintf(ffp, "<?xml version=\"1.0\" encoding=\"Shift_JIS\"?>\n");
    fprintf(ffp, "<gaijiData xml:space=\"preserve\">\n");
    message("外字ファイルを生成しています... ");
    first = TRUE;
    zg_start_unicode = 0xe000;
    zg_start_ebhigh = 0xa1;
    if ((fp = fopen(ZGAIJI_FILE, "r")) != NULL) {
	unicode = zg_start_unicode;
	ebhigh = zg_start_ebhigh;
	eblow = 0x21;
	while (fgets(buf, MAX_DLINE, fp) != NULL) {
	    if (*buf == ' ' || *buf == '#') {
		fputs(buf, ffp);
		continue;
	    }
	    if (!strncmp(buf, "<fontSet", 8)) {
		p = strstr(buf, "start=");
		zg_orig_ebhigh = hexval(p+7);
		zg_orig_eblow = hexval(p+9);
	    }
	    if (strncmp(buf, "<fontData", 9))
		continue;
	    if (first) {
		fprintf(ffp, "<fontSet size=\"16X16\" start=\"%02X%02X\">\n",
		    ebhigh, eblow);
		first = FALSE;
	    } else
		fprintf(ffp, "</fontData>\n");
	    fprintf(ffp, "<fontData ebcode=\"%02X%02X\" unicode=\"#x%04X\">\n",
		ebhigh, eblow, unicode);
	    fprintf(mfp, "<gaijiMap unicode=\"#x%04X\" ebcode=\"%02X%02X\"/>\n",
		unicode, ebhigh, eblow);
	    unicode++;
	    if (eblow < 0x7e) {
		eblow++;
	    } else {
		eblow = 0x21;
		ebhigh++;
	    }
	}
	fclose(fp);
	if (!first) {
	    fprintf(ffp, "</fontData>\n");
	    fprintf(ffp, "</fontSet>\n");
	}
    }
    first = TRUE;
    hg_start_unicode = unicode;
    hg_start_ebhigh = ebhigh + 1;
    if ((fp = fopen(HGAIJI_FILE, "r")) != NULL) {
	unicode = hg_start_unicode;
	ebhigh = hg_start_ebhigh;
	eblow = 0x21;
	while (fgets(buf, MAX_DLINE, fp) != NULL) {
	    if (*buf == ' ' || *buf == '#') {
		fputs(buf, ffp);
		continue;
	    }
	    if (!strncmp(buf, "<fontSet", 8)) {
		p = strstr(buf, "start=");
		hg_orig_ebhigh = hexval(p+7);
		hg_orig_eblow = hexval(p+9);
	    }
	    if (strncmp(buf, "<fontData", 9))
		continue;
	    if (first) {
		fprintf(ffp, "<fontSet size=\"8X16\" start=\"%02X%02X\">\n",
		    ebhigh, eblow);
		first = FALSE;
	    } else
		fprintf(ffp, "</fontData>\n");
	    fprintf(ffp, "<fontData ebcode=\"%02X%02X\" unicode=\"#x%04X\">\n",
		ebhigh, eblow, unicode);
	    fprintf(mfp, "<gaijiMap unicode=\"#x%04X\" ebcode=\"%02X%02X\"/>\n",
		unicode, ebhigh, eblow);
	    unicode++;
	    if (eblow < 0x7e) {
		eblow++;
	    } else {
		eblow = 0x21;
		ebhigh++;
	    }
	}
	fclose(fp);
	if (!first) {
	    fprintf(ffp, "</fontData>\n");
	    fprintf(ffp, "</fontSet>\n");
	}
    }
    fprintf(ffp, "</gaijiData>\n");
    fprintf(mfp, "</gaijiSet>\n");
    fclose(ffp);
    fclose(mfp);
    message("終了しました\n");
    return OK;
}

/*
 * 文字列を外字のUnicode表記に変換する
 */
byte *
gstr(byte *str, int halfwidth)
{
    int high, low, code;
    static byte buf[MAX_WORD];

    if (*str == '<')
	str++;
    high = hexval(str);
    low = hexval(str+2);
    if (high < 0xa1) {
	sprintf(buf, "&#x%02X;&#x%02X;", high, low);
	return buf;
    }
    if (halfwidth) {
	code = hg_start_unicode;
	if (low < hg_orig_eblow) {
	    low += 94;
	    high--;
	}
	code += (high - hg_orig_ebhigh) * 94 + (low - hg_orig_eblow);
    } else {
	code = zg_start_unicode;
	if (low < zg_orig_eblow) {
	    low += 94;
	    high--;
	}
	code += (high - zg_orig_ebhigh) * 94 + (low - zg_orig_eblow);
    }
    sprintf(buf, "&#x%04X;", code);
    return buf;
}

/*
 * 見出し文字列をシフトJIS文字列に変換する
 */
byte *
conv_title(byte *dst, byte *src)
{
    int n, halfwidth;
    byte *p, *q;

    halfwidth = FALSE;
    p = src;
    q = dst;
    while (*p) {
	if (*p == '<') {
	    if (p[1] >= 'A') {
		/*
		 * 外字
		 */
		q = addstr(q, gstr(p, halfwidth));
	    } else if (!strncmp(p+1, "1F04", 4)) {
		/*
		 * 半角開始
		 */
		halfwidth = TRUE;
	    } else if (!strncmp(p+1, "1F05", 4)) {
		/*
		 * 半角終了
		 */
		halfwidth = FALSE;
	    } else {
		/*
		 * その他のタグはとりあえず無視
		 */
	    }
	    p += 6;
	    continue;
	}
	if (halfwidth && (n = tohan(p)) != ERR) {
	    switch (n) {
	    case '<':
		q = addstr(q, "&lt;");
		break;
	    case '>':
		q = addstr(q, "&gt;");
		break;
	    case '&':
		q = addstr(q, "&amp;");
		break;
	    case '"':
		q = addstr(q, "&quot;");
		break;
	    case '\'':
		q = addstr(q, "&apos;");
		break;
	    default:
		*q++ = n;
		break;
	    }
	    p += 2;
	} else {
	    *q++ = *p++;
	    *q++ = *p++;
	}
    }
    *q = '\0';
    return dst;
}

/*
 * インデックスデータを変換する
 */
int
convert_index_data(FILE *ifp, FILE *ofp)
{
    int complex, n, len, firsterr;
    dword dblk, tblk;
    word dpos, tpos;
    byte *p, buf[MAX_DLINE], str[MAX_WORD], tmp[MAX_WORD];

    firsterr = TRUE;
    complex = FALSE;
    while ((p = fgets(buf, MAX_DLINE, ifp)) != NULL) {
	if (strncmp(p, "ID=", 3))
	    continue;
	if (!strncmp(p+3, "C0", 2))
	    break;
	if (!strncmp(p+3, "D0", 2)) {
	    complex = TRUE;
	    break;
	}
    }
    if (p == NULL)
	return ERR;
    while ((p = fgets(buf, MAX_DLINE, ifp)) != NULL) {
	if (*p == '\r' || *p == '\n' ||
	    !strncmp(p, "block#", 6) ||
	    !strncmp(p, "ID=", 3)) {
	    continue;
	}
	if (complex) {
	    if (!strncmp(p, "80:", 3))
		continue;
	    if (!strncmp(p, "C0:", 3) || !strncmp(p, "00:", 3))
		p += 3;
	}
	if (*p == '[') {
	    /*
	     * 索引語が空っぽ。外字か?
	     * (広辞苑第五版の表記インデックス末尾にある)
	     */
	    continue;
	}
	p = getupto(p, tmp, '[');
	n = sscanf(p, "[%d]\t[%lx:%x][%lx:%x]",
	    &len, &dblk, &dpos, &tblk, &tpos);
	if (n != 5) {
	    if (firsterr) {
		write_log("\n");
		firsterr = FALSE;
	    }
	    write_log("不正なインデックス行: %s", buf);
	    continue;
	}
	if (dpos == 0x0800) {
	    dpos = 0;
	    dblk++;
	};
	if (tpos == 0x0800) {
	    tpos = 0;
	    tblk++;
	}
	fprintf(ofp, "%08lX|%04X|%08lX|%04X|%s|\n",
	    dblk, dpos, tblk, tpos, conv_title(str, tmp));
    }
    return OK;
}

/*
 * 見出しデータを変換する
 */
int
convert_title_data(FILE *ifp, int ofd)
{
    int n, first, firsterr;
    long pos;
    dword tblk, start_block;
    word tpos;
    byte *p, *q, buf[MAX_DLINE], str[MAX_WORD];

    first = TRUE;
    firsterr = TRUE;
    while ((p = fgets(buf, MAX_DLINE, ifp)) != NULL) {
	if (*p == '\r' || *p == '\n' || !strncmp(p, "[ID=", 4))
	    continue;
	if (sscanf(p, "[%lx:%x]", &tblk, &tpos) != 2) {
	    if (firsterr) {
		write_log("\n");
		firsterr = FALSE;
	    }
	    write_log("不正な見出し行: %s", buf);
	    continue;
	}
	p = skipch(p, ']');
	if (*p == '\r' || *p == '\n' ||
	    !strncmp(p, "<1F02>", 6) ||
	    !strncmp(p, "<1F03>", 6)) {
	    continue;
	}
	if (first) {
	    start_block = tblk;
	    first = FALSE;
	}
	q = p;
	while (*q && *q != '\r' && *q != '\n')
	    q++;
	*q = '\0';
	n = strlen(p);
	if (n > 6 && !strncmp(&p[n-6], "<1F0A>", 6))
	    p[n-6] = '\0';
	conv_title(str, p);
	pos = (long)((tblk - start_block) * 2048 + tpos) * 4;
	n = strlen(str) + 1;
	if (lseek(ofd, pos, SEEK_SET) < 0)
	    return ERR;
	if (write(ofd, str, n) != n)
	    return ERR;
    }
    return (int)start_block;
}

/*
 * インデックスおよび見出しの作業データファイルを生成する
 */
int
generate_work_file(void)
{
    int ofd, n;
    FILE *ifp, *ofp;
    byte buf[MAX_PATH];
    struct stat st;

    if (stat(FKINDEX_FILE, &st) == 0) {
	if ((ifp = fopen(FKINDEX_FILE, "r")) == NULL) {
	    message("かなインデックスファイル %s がオープンできません\n",
		FKINDEX_FILE);
	    return ERR;
	}
	if ((ofp = fopen(FKINDEX_WORK, "w")) == NULL) {
	    message("かなインデックス作業ファイル %s が新規作成できません\n",
		FKINDEX_WORK);
	    return ERR;
	}
	message("かなインデックスデータを変換しています...");
	if (convert_index_data(ifp, ofp) == ERR) {
	    message("かなインデックスデータの変換に失敗しました\n");
	    return ERR;
	}
	message(" 終了しました\n");
	fclose(ifp);
	fclose(ofp);
	message("かなインデックスデータをソートしています...");
	sprintf(buf, "%s %s > %s", sort_cmd, FKINDEX_WORK, FKINDEX_DATA);
	system(buf);
	unlink(FKINDEX_WORK);
	message(" 終了しました\n");
    }
    if (stat(FKTITLE_FILE, &st) == 0) {
	if ((ifp = fopen(FKTITLE_FILE, "r")) == NULL) {
	    message("かな見出しファイル %s がオープンできません\n",
		FKTITLE_FILE);
	    return ERR;
	}
	if ((ofd = open(FKTITLE_DATA, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0644)) < 0) {
	    message("かな見出しデータファイル %s が新規作成できません\n",
		FKTITLE_DATA);
	    return ERR;
	}
	message("かな見出しデータを生成しています...");
	if ((n = convert_title_data(ifp, ofd)) == ERR) {
	    message("かな見出しデータの生成に失敗しました\n");
	    return ERR;
	}
	fktitle_start_block = n;
	message(" 終了しました\n");
	fclose(ifp);
	close(ofd);
    }
    if (stat(FHINDEX_FILE, &st) == 0) {
	if ((ifp = fopen(FHINDEX_FILE, "r")) == NULL) {
	    message("表記インデックスファイル %s がオープンできません\n",
		FHINDEX_FILE);
	    return ERR;
	}
	if ((ofp = fopen(FHINDEX_WORK, "w")) == NULL) {
	    message("表記インデックス作業ファイル %s が新規作成できません\n",
		FHINDEX_WORK);
	    return ERR;
	}
	message("表記インデックスデータを変換しています...");
	if (convert_index_data(ifp, ofp) == ERR) {
	    message("表記インデックスデータの変換に失敗しました\n");
	    return ERR;
	}
	message(" 終了しました\n");
	fclose(ifp);
	fclose(ofp);
	message("表記インデックスデータをソートしています...");
	sprintf(buf, "%s %s > %s", sort_cmd, FHINDEX_WORK, FHINDEX_DATA);
	system(buf);
	unlink(FHINDEX_WORK);
	message(" 終了しました\n");
    }
    if (stat(FHTITLE_FILE, &st) == 0) {
	if ((ifp = fopen(FHTITLE_FILE, "r")) == NULL) {
	    message("表記見出しファイル %s がオープンできません\n",
		FHTITLE_FILE);
	    return ERR;
	}
	if ((ofd = open(FHTITLE_DATA, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0644)) < 0) {
	    message("表記見出しデータファイル %s が新規作成できません\n",
		FHTITLE_DATA);
	    return ERR;
	}
	message("表記見出し作業データを生成しています...");
	if ((n = convert_title_data(ifp, ofd)) == ERR) {
	    message("表記見出しデータの生成に失敗しました\n");
	    return ERR;
	}
	fhtitle_start_block = n;
	message(" 終了しました\n");
	fclose(ifp);
	close(ofd);
    }
    if (stat(FAINDEX_FILE, &st) == 0) {
	if ((ifp = fopen(FAINDEX_FILE, "r")) == NULL) {
	    message("英字インデックスファイル %s がオープンできません\n",
		FAINDEX_FILE);
	    return ERR;
	}
	if ((ofp = fopen(FAINDEX_WORK, "w")) == NULL) {
	    message("英字インデックス作業ファイル %s が新規作成できません\n",
		FAINDEX_WORK);
	    return ERR;
	}
	message("英字インデックスデータを変換しています...");
	if (convert_index_data(ifp, ofp) == ERR) {
	    message("英字インデックスデータの変換に失敗しました\n");
	    return ERR;
	}
	message(" 終了しました\n");
	fclose(ifp);
	fclose(ofp);
	message("英字インデックスデータをソートしています...");
	sprintf(buf, "%s %s > %s", sort_cmd, FAINDEX_WORK, FAINDEX_DATA);
	system(buf);
	unlink(FAINDEX_WORK);
	message(" 終了しました\n");
    }
    if (stat(FATITLE_FILE, &st) == 0) {
	if ((ifp = fopen(FATITLE_FILE, "r")) == NULL) {
	    message("英字見出しファイル %s がオープンできません\n",
		FATITLE_FILE);
	    return ERR;
	}
	if ((ofd = open(FATITLE_DATA, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0644)) < 0) {
	    message("英字見出しデータファイル %s が新規作成できません\n",
		FATITLE_DATA);
	    return ERR;
	}
	message("英字見出し作業データを生成しています...");
	if ((n = convert_title_data(ifp, ofd)) == ERR) {
	    message("英字見出しデータの生成に失敗しました\n");
	    return ERR;
	}
	fatitle_start_block = n;
	message(" 終了しました\n");
	fclose(ifp);
	close(ofd);
    }
    return OK;
}

/*
 * HTMLファイルを新規作成する
 */
FILE *
html_newfile(void)
{
    byte buf[MAX_PATH];
    FILE *fp;

    sprintf(buf, "%s%s", base_path, html_file);
    if ((fp = fopen(buf, "w")) == NULL) {
	message("HTMLファイル %s が新規作成できません\n", buf);
	return NULL;
    }
    fprintf(fp, "<html>\n");
    fprintf(fp, "<head>\n");
    fprintf(fp, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=x-sjis\">\n");
    fprintf(fp, "<meta name=\"GENERATOR\" content=\"%s %s %s\">\n",
	progname, version, date);
    fprintf(fp, "<title>Dictionary</title>\n");
    fprintf(fp, "</head>\n");
    fprintf(fp, "<body>\n");
    fprintf(fp, "<dl>\n");
    return fp;
}

/*
 * HTMLファイルをクローズする
 */
int
html_close(FILE *fp)
{
    fprintf(fp, "</dl>\n");
    fprintf(fp, "</body>\n");
    fprintf(fp, "</html>\n");
    return fclose(fp);
}

/*
 * インデックスと見出しを1件読む
 */
INDEX *
read_index_data(FILE *fp, int fd, dword topblk, INDEX *ip)
{
    int n;
    long pos;
    byte *p, buf[MAX_DLINE];

    if ((p = fgets(buf, MAX_DLINE, fp)) == NULL)
	return NULL;
    n = sscanf(p, "%lx|%x|%lx|%x", &ip->dblk, &ip->dpos, &ip->tblk, &ip->tpos);
    if (n != 4)
	return NULL;
    p += 28;	/* "xxxxxxxx|xxxx|yyyyyyyy|yyyy|"をスキップ */
    getupto(p, ip->str, '|');
    if (ip->dblk == ip->tblk && ip->dpos == ip->tpos) {
	/*
	 * 見出しデータは本文の見出し行を共用する(広辞苑第五版)
	 */
	*ip->title = '\0';
    } else {
	pos = (long)((ip->tblk - topblk) * 2048 + ip->tpos) * 4;
	if (lseek(fd, pos, SEEK_SET) < 0)
	    return NULL;
	if (read(fd, ip->title, MAX_WORD) <= 0)
	    return NULL;
    }
    return ip;
}

/*
 * 本文を1行読む
 */
HONMON *
read_honmon_data(FILE *fp, HONMON *hp)
{
    byte *p, *q, buf[MAX_BUFF];

    if (fgets(buf, MAX_BUFF, fp) == NULL)
	return NULL;
    if (sscanf(buf, "[%lx:%x]", &hp->dblk, &hp->dpos) != 2) {
	hp->dblk = 0L;
	hp->dpos = 0;
	return hp;
    }
    if (hp->dpos == 0x0800) {
	hp->dpos = 0;
	hp->dblk++;
    }
    p = skipch(buf, ']');
    q = p;
    while (*q && *q != '\r' && *q != '\n')
	q++;
    *q = '\0';
    strcpy(hp->buf, p);
    return hp;
}

/*
 * インデックスの参照先と本文データの位置関係を比較する
 */
int
compare_position(INDEX *ip, HONMON *dp)
{
    byte *p;
    dword ipos, dpos;

    ipos = ip->dblk * 2048 + ip->dpos;
    dpos = dp->dblk * 2048 + dp->dpos;
    /*
     * 本文の行頭から
     *   <1F09><xxxx>
     *   <1F41><xxxx>
     *   <1F61>
     * の並びが連続していて、
     * インデックスの参照先がその直後の場合は
     * インデックスがその行頭を参照しているとみなす
     * (ジーニアス英和辞典など)
     */
    if (ipos < dpos)
	return -1;
    p = dp->buf;
    while (*p && ipos > dpos) {
	if (!strncmp(p, "<1F09>", 6)) {
	    p += 12;
	    dpos += 4;
	    continue;
	}
	if (!strncmp(p, "<1F41>", 6)) {
	    p += 12;
	    dpos += 4;
	}
	if (!strncmp(p, "<1F61>", 6)) {
	    p += 6;
	    dpos += 2;
	}
	break;
    }
    if (ipos > dpos)
	return 1;
    return 0;
}

/*
 * 本文データを変換する
 */
byte *
conv_honmon(byte *dst, byte *src)
{
    int n, halfwidth;
    dword dblk;
    word dpos;
    byte *p, *q, *r, *linetop;
    byte endtag[MAX_WORD], buf[MAX_DLINE], tmp[MAX_DLINE];

    halfwidth = FALSE;
    p = src;
    q = dst;
    linetop = q;
    while (*p) {
	if (*p == '<') {
	    if (p[1] == '1' && p[2] == 'F') {	/* 1Fxx */
		switch (p[3]) {
		case '0':
		    switch (p[4]) {
		    case '4':	/* 1F04: 半角開始 */
			halfwidth = TRUE;
			p += 6;
			break;
		    case '5':	/* 1F05: 半角終了 */
			halfwidth = FALSE;
			p += 6;
			break;
		    case '6':	/* 1F06: 下添え字開始 */
			q = addstr(q, "<sub>");
			p += 6;
			break;
		    case '7':	/* 1F07: 下添え字終了 */
			q = addstr(q, "</sub>");
			p += 6;
			break;
		    case 'A':	/* 1F0A: 改行 */
			q = addstr(q, "<br>");
			p += 6;
			break;
		    case 'E':	/* 1F0E: 上添え字開始 */
			q = addstr(q, "<sup>");
			p += 6;
			break;
		    case 'F':	/* 1F0F: 上添え字終了 */
			q = addstr(q, "</sup>");
			p += 6;
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		case '1':
		    switch (p[4]) {
		    case '0':	/* 1F10: 分割禁止開始 */
			q = addstr(q, "<nobr>");
			p += 6;
			break;
		    case '1':	/* 1F11: 分割禁止終了 */
			q = addstr(q, "</nobr>");
			p += 6;
			break;
		    case '4':	/* 1F14 ... 1F15: 色見本 */
			/*
			 * "[色見本]"で置き換える
			 */
			p = skipstr(p+6, "<1F15>");
			q = addstr(q, "[色見本]");
			break;
		    case 'A':	/* 1F1A xxxx: タブ位置指定 */
		    case 'B':	/* 1F1B xxxx: 字下げ・字上がり指定 */
		    case 'C':	/* 1F1C xxxx: センタリング指定 */
			p += 12;
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		case '3':
		    switch (p[4]) {
		    case '9':	/* 1F39 ... 1F59: カラー動画表示 */
			/*
			 * "[動画]"で置き換える
			 */
			p = skipstr(p+6, "<1F59>");
			q = addstr(q, "[動画]");
			break;
		    case 'C':	/* 1F3C ... 1F5C: インライン画像参照 */
			/*
			 * "[図版]"で置き換える
			 */
			p = skipstr(p+6, "<1F5C>");
			q = addstr(q, "[図版]");
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		case '4':
		    switch (p[4]) {
		    case '1':	/* 1F41 xxxx: 検索キー開始 */
			p += 12;
			break;
		    case '2':	/* 1F42 text 1F62[xx:yy]: 別項目参照 */
			p += 6;
			while(!strncmp(p, "<1F42>", 2))p+=6;
			while(!strncmp(p, "→", 2))p+=2;
			r = strstr(p, "<1F62>");
			n = r - p;
			strncpy(buf, p, n);
			buf[n] = '\0';
			p = r + 6;
			sscanf(p, "[%lx:%x]", &dblk, &dpos);
			p = skipch(p, ']');
			sprintf(q, "<a href=\"#%08lX%04X\">%s</a>",
			    dblk, dpos, conv_honmon(tmp, buf));
			q = endstr(q);
			break;
		    case '4':	/* 1F44 ... 1F64[xx:yy]: 図版参照 */
			/*
			 * "[図版]"で置き換える
			 */
			p = skipstr(p+6, "<1F64>");
			p = skipch(p, ']');
			q = addstr(q, "[図版]");
			break;
		    case 'A':	/* 1F4A ... 1F6A: 音声参照 */
			/*
			 * "[音声]"で置き換える
			 */
			p = skipstr(p+6, "<1F6A>");
			q = addstr(q, "[音声]");
			break;
		    case '5':	/* 1F45 ... 1F65: 図版見出し */
		    case 'B':	/* 1F4B ... 1F6B: カラー画像データ群参照 */
		    case 'C':	/* 1F4C ... 1F6C: カラー画面データ群参照 */
		    case 'D':	/* 1F4D ... 1F6D: カラー画面表示 */
		    case 'F':	/* 1F4F ... 1F6F: カラー画面参照 */
			/*
			 * "[図版]"で置き換える
			 */
			sprintf(endtag, "<1F6%c>", p[4]);
			p = skipstr(p+6, endtag);
			q = addstr(q, "[図版]");
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		case '6':
		    switch (p[4]) {
		    case '1':	/* 1F61: 検索キー終了 */
			p += 6;
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		case 'E':
		    switch (p[4]) {
		    case '0':	/* 1FE0 xxxx ... 1FE1: 文字修飾 */
			p += 12;
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		default:
		    p += 6;
		    break;
		}
	    } else if (p[1] >= 'A') {
		/*
		 * 外字
		 */
		q = addstr(q, gstr(p, halfwidth));
		p += 6;
	    } else {
		/*
		 * その他のタグは無視
		 */
		p += 6;
	    }
	    continue;
	}
	if (halfwidth && (n = tohan(p)) != ERR) {
	    switch (n) {
	    case '<':
		q = addstr(q, "&lt;");
		break;
	    case '>':
		q = addstr(q, "&gt;");
		break;
	    case '&':
		q = addstr(q, "&amp;");
		break;
	    case '"':
		q = addstr(q, "&quot;");
		break;
	    case '\'':
		q = addstr(q, "&apos;");
		break;
	    default:
		*q++ = n;
		break;
	    }
	    p += 2;
	    if (q - linetop >= MAX_HLINE && n == '.') {
		*q++ = '\n';
		linetop = q;
	    }
	} else {
	    *q++ = *p++;
	    *q++ = *p++;
	    if (q - linetop >= MAX_HLINE && !strncmp(q-2, "。", 2)) {
		*q++ = '\n';
		linetop = q;
	    }
	}
    }
    if (q >= dst + 4 && !strncmp(q-4, "<br>", 4)) {
	/*
	 * 最後の<br>は捨てる
	 */
	q -= 4;
    }
    *q = '\0';
    return dst;
}

/*
 * 「1F41 xxxx ～ 1F61」または「1F41 xxxx 1F61 1FE0 xxxx ～ 1FE1」で
 * 囲まれたタイトルを取り出し、直後の位置を返す
 * ただし囲まれた範囲の長さが128バイト以上か
 * <1F61>が見つからない場合はタイトルとしない
 */
byte *
get_title(byte *str, byte *title)
{
    int n;
    byte *p, *r;

    if (strncmp(str, "<1F41>", 6)) {
	*title = '\0';
	return str;
    }
    p = str + 12;	/* <1F41><xxxx>をスキップ */
    r = strstr(p, "<1F61>");
    if (r == NULL) {
	*title = '\0';
	return str;
    }
    n = r - p;
    if (n >= MAX_WORD) {
	*title = '\0';
	return str;
    }
    strncpy(title, p, n);
    title[n] = '\0';
    p = r + 6;
    if (n == 0 && !strncmp(p, "<1FE0>", 6)) {
	/*
	 * <1F41><xxxx><1F61><1FE0><yyyy> ～ <1FE1>の形式
	 */
	p += 12;
	r = strstr(p, "<1FE1>");
	if (r == NULL) {
	    *title = '\0';
	    return str;
	}
	n = r - p;
	strncpy(title, p, n);
	title[n] = '\0';
	p = r + 6;
    }
    return p;
}

/*
 * 入力の字下げタグをスキップし、字下げ量を返す
 */
byte *
skipindent(byte *str, int *indentp)
{
    if (strncmp(str, "<1F09>", 6)) {
	*indentp = 0;
	return str;
    }
    *indentp = hexval(str+7) * 256 + hexval(str+9) - 1;
    return str + 12;
}

/*
 * 出力用字下げタグ文字列を作る
 */
byte *
indentstr(int indent)
{
    static byte buf[32];

    sprintf(buf, "&#x1f09;&#x00;&#x%02x;", indent+1);
    return buf;
}

/*
 * honmon.txtと作業データファイルを突き合わせてHTMLファイルを生成する
 */
int
generate_html_file(void)
{
    int first_dt, needbr, indent, indent2;
    int have_preamble, have_indent, have_indent2;
    int yield_dt, new_content;
    int ktfd, htfd, atfd;
    dword ktop, htop, atop;
    byte *p, *title;
    byte buf[MAX_BUFF], tbuf[MAX_DLINE], tmp[MAX_DLINE];
    byte istr[MAX_WORD], istr2[MAX_WORD];
    FILE *fp, *honfp, *kifp, *hifp, *aifp;
    INDEX *kp, *hp, *ap, kidx, hidx, aidx;
    HONMON *dp, honmon;

    if ((fp = html_newfile()) == NULL)
	return ERR;
    if ((honfp = fopen(HONMON_FILE, "r")) == NULL) {
	message("本文ファイル %s がオープンできません\n", HONMON_FILE);
	return ERR;
    }
    gen_kana = FALSE;
    if ((kifp = fopen(FKINDEX_DATA, "r")) != NULL &&
	(ktfd = open(FKTITLE_DATA, O_RDONLY|O_BINARY)) != -1) {
	gen_kana = TRUE;
    }
    gen_hyoki = FALSE;
    if ((hifp = fopen(FHINDEX_DATA, "r")) != NULL &&
	(htfd = open(FHTITLE_DATA, O_RDONLY|O_BINARY)) != -1) {
	gen_hyoki = TRUE;
    }
    gen_alpha = FALSE;
    if ((aifp = fopen(FAINDEX_DATA, "r")) != NULL &&
	(atfd = open(FATITLE_DATA, O_RDONLY|O_BINARY)) != -1) {
	gen_alpha = TRUE;
    }
    if (!gen_kana && !gen_hyoki && !gen_alpha) {
	message("かな/表記/英字いずれのインデックス/見出しもありません\n");
	message("HTMLファイルの生成を中止します\n");
	return ERR;
    }
    message("HTMLファイルを生成しています...\n");
    have_auto_kana = FALSE;
    have_preamble = FALSE;
    first_dt = TRUE;
    needbr = FALSE;
    new_content = FALSE;
    kp = NULL;
    hp = NULL;
    ap = NULL;
    if (gen_kana) {
	ktop = fktitle_start_block;
	kp = read_index_data(kifp, ktfd, ktop, &kidx);
    }
    if (gen_hyoki) {
	htop = fhtitle_start_block;
	hp = read_index_data(hifp, htfd, htop, &hidx);
    }
    if (gen_alpha) {
	atop = fatitle_start_block;
	ap = read_index_data(aifp, atfd, atop, &aidx);
    }
    while ((dp = read_honmon_data(honfp, &honmon)) != NULL) {
	if (dp->dblk == 0L || *dp->buf == '\0')
	    continue;
	p = dp->buf;
	if (!strcmp(p, "<1F02>") || !strcmp(p, "<1F03>")) {
	    /*
	     * これらはどこにあっても単独で出力する
	     */
	    if (needbr) {
		fprintf(fp, "<br>\n");
		needbr = FALSE;
	    }
	    if (!strcmp(p, "<1F02>")) {
		fprintf(fp, "&#x1F02;\n");
		new_content = TRUE;
	    } else {
		fprintf(fp, "&#x1F03;\n");
	    }
	    needbr = FALSE;
	    continue;
	}
	if (new_content) {
	    /*
	     * <1F02>の直後の行; 無条件にアンカーを作る
	     * (研究社新英和中辞典など)
	     */
	    fprintf(fp, "<a name=\"%08lX%04X\"></a>\n", dp->dblk, dp->dpos);
	    new_content = FALSE;
	}
	have_indent = FALSE;
	have_indent2 = FALSE;
	*istr = '\0';
	*istr2 = '\0';
	if (!strncmp(dp->buf, "<1F09>", 6)) {
	    p = skipindent(p, &indent);
	    have_indent = TRUE;
	    strcpy(istr, indentstr(indent));
	}
	if (!strncmp(p, "<1F41>", 6)) {
	    p = get_title(p, tmp);
	    if (!strncmp(p, "<1F09>", 6)) {
		p = skipindent(p, &indent2);
		have_indent2 = TRUE;
		strcpy(istr2, indentstr(indent2));
	    }
	    conv_honmon(tbuf, tmp);
	    conv_honmon(buf, p);
	    title = *tbuf? tbuf: buf;
	} else {
	    *tbuf = '\0';
	    title = conv_honmon(buf, p);
	}
	while (kp && compare_position(kp, dp) < 0) {
	    message("使われないかなインデックスがあります\n");
	    message("  本文位置=[%08lX:%04X], インデックス=[%08lX:%04X]%s\n",
		dp->dblk, dp->dpos, kp->dblk, kp->dpos, kp->str);
	    kp = read_index_data(kifp, ktfd, ktop, &kidx);
	}
	while (hp && compare_position(hp, dp) < 0) {
	    message("使われない表記インデックスがあります\n");
	    message("  本文位置=[%08lX:%04X], インデックス=[%08lX:%04X]%s\n",
		dp->dblk, dp->dpos, hp->dblk, hp->dpos, hp->str);
	    hp = read_index_data(hifp, htfd, htop, &hidx);
	}
	while (ap && compare_position(ap, dp) < 0) {
	    message("使われない英字インデックスがあります\n");
	    message("  本文位置=[%08lX:%04X], インデックス=[%08lX:%04X]%s\n",
		dp->dblk, dp->dpos, ap->dblk, ap->dpos, ap->str);
	    ap = read_index_data(aifp, atfd, atop, &aidx);
	}
	if (kp && compare_position(kp, dp) == 0 ||
	    hp && compare_position(hp, dp) == 0 ||
	    ap && compare_position(ap, dp) == 0) {
	    /*
	     * インデックスから参照されている
	     */
	    if (have_preamble) {
		fprintf(fp, "\n</p>\n");
		have_preamble = FALSE;
	    }
	    if (have_indent && indent > 0 || strlen(title) > MAX_WORD) {
		/*
		 * 字下げされた位置が参照されているか
		 * あるいはタイトルが128バイトを超えているので
		 * 見出しとしては採用しない
		 * <dd>～</dd>内に参照位置があったとみなし、
		 * あたらしい<p>を始める
		 */
		yield_dt = FALSE;
	    } else {
		/*
		 * 行頭位置が参照されており
		 * タイトル長が128バイト以下なので
		 * 新しい<dt id=...>を始める
		 */
		yield_dt = TRUE;
	    }
	    if (yield_dt) {
		if (first_dt)
		    first_dt = FALSE;
		else
		    fprintf(fp, "\n</p></dd>\n");
		fprintf(fp, "<dt id=\"%08lX%04X\">%s</dt>\n",
		    dp->dblk, dp->dpos, title);
	    } else {
		fprintf(fp, "\n</p>\n<p>\n");
	    }
	    while (kp && compare_position(kp, dp) == 0) {
		fprintf(fp, "<key title=\"%s\" type=\"かな\">%s</key>\n",
		    *kp->title? kp->title: title, kp->str);
		kp = read_index_data(kifp, ktfd, ktop, &kidx);
	    }
	    while (hp && compare_position(hp, dp) == 0) {
		fprintf(fp, "<key title=\"%s\" type=\"表記\">%s</key>\n",
		    *hp->title? hp->title: title, hp->str);
		if (auto_kana && iskanastr(hp->str)) {
		    fprintf(fp, "<key title=\"%s\" type=\"かな\">%s</key>\n",
			*hp->title? hp->title: title, hp->str);
		    have_auto_kana = TRUE;
		}
		hp = read_index_data(hifp, htfd, htop, &hidx);
	    }
	    while (ap && compare_position(ap, dp) == 0) {
		fprintf(fp, "<key title=\"%s\" type=\"表記\">%s</key>\n",
		    *ap->title? ap->title: title, ap->str);
		ap = read_index_data(aifp, atfd, atop, &aidx);
	    }
	    if (yield_dt) {
		fprintf(fp, "<dd><p>\n");
		if (*tbuf && *buf)
		    fprintf(fp, "%s%s", istr2, buf);
		else
		    fprintf(fp, " ");
	    } else {
		fprintf(fp, "%s%s%s%s", istr, tbuf, istr2, buf);
	    }
	    have_indent = FALSE;
	    have_indent2 = FALSE;
	    *istr = '\0';
	    *istr2 = '\0';
	    needbr = TRUE;
	    continue;
	}
	if (needbr) {
	    fprintf(fp, "<br>\n");
	    needbr = FALSE;
	}
	if (first_dt && !have_preamble) {
	    /*
	     * 最初の<dt ...>より前に内容があった
	     */
	    fprintf(fp, "<p>\n");
	    have_preamble = TRUE;
	}
	fprintf(fp, "%s%s%s%s", istr, tbuf, istr2, buf);
	needbr = TRUE;
    }
    fprintf(fp, "\n");
    if (gen_kana) {
	fclose(kifp);
	close(ktfd);
    }
    if (gen_hyoki) {
	fclose(hifp);
	close(htfd);
    }
    if (gen_alpha) {
	fclose(aifp);
	close(atfd);
    }
    fclose(honfp);
    html_close(fp);
    message("HTMLファイルの生成が終了しました\n");
    return OK;
}

/*
 * EBStudio定義ファイルを生成する
 */
int
generate_ebs_file(void)
{
    byte buf[MAX_PATH];
    FILE *fp;

    sprintf(buf, "%s%s", base_path, ebs_file);
    if ((fp = fopen(buf, "w")) == NULL) {
	message("EBStudio定義ファイル %s が新規作成できません\n", buf);
	return ERR;
    }
    message("EBSファイルを生成しています... ");
    fprintf(fp, "InPath=%s\n", base_path);
    fprintf(fp, "OutPath=%s\n", out_path);
    fprintf(fp, "IndexFile=\n");
    fprintf(fp, "Copyright=\n");
    fprintf(fp, "GaijiFile=$(BASE)\\%s\n", GFONT_FILE);
    fprintf(fp, "GaijiMapFile=$(BASE)\\%s\n", GMAP_FILE);
    fprintf(fp, "EBType=%d\n", eb_type);
    fprintf(fp, "WordSearchHyoki=%d\n", (gen_hyoki || gen_alpha)? 1: 0);
    fprintf(fp, "WordSearchKana=%d\n", (gen_kana || have_auto_kana)? 1: 0);
    fprintf(fp, "EndWordSearchHyoki=%d\n", (gen_hyoki || gen_alpha)? 1: 0);
    fprintf(fp, "EndWordSearchKana=%d\n", (gen_kana || have_auto_kana)? 1: 0);
    fprintf(fp, "KeywordSearch=0\n");
    fprintf(fp, "ComplexSearch=0\n");
    fprintf(fp, "topMenu=0\n");
    fprintf(fp, "singleLine=1\n");
    fprintf(fp, "kanaSep1=【\n");
    fprintf(fp, "kanaSep2=】\n");
    fprintf(fp, "hyokiSep=0\n");
    fprintf(fp, "makeFig=0\n");
    fprintf(fp, "inlineImg=0\n");
    fprintf(fp, "paraHdr=0\n");
    fprintf(fp, "ruby=1\n");
    fprintf(fp, "paraBr=0\n");
    fprintf(fp, "subTitle=0\n");
    fprintf(fp, "dfnStyle=0\n");
    fprintf(fp, "srchUnit=1\n");
    fprintf(fp, "linkChar=0\n");
    fprintf(fp, "arrowCode=222A\n");
    fprintf(fp, "eijiPronon=1\n");
    fprintf(fp, "eijiPartOfSpeech=1\n");
    fprintf(fp, "eijiBreak=1\n");
    fprintf(fp, "eijiKana=0\n");
    fprintf(fp, "leftMargin=0\n");
    fprintf(fp, "indent=0\n");
    fprintf(fp, "tableWidth=480\n");
    fprintf(fp, "StopWord=\n");
    fprintf(fp, "delBlank=1\n");
    fprintf(fp, "delSym=1\n");
    fprintf(fp, "delChars=\n");
    fprintf(fp, "refAuto=0\n");
    fprintf(fp, "titleWord=0\n");
    fprintf(fp, "autoWord=0\n");
    fprintf(fp, "autoEWord=0\n");
    fprintf(fp, "HTagIndex=0\n");
    fprintf(fp, "DTTagIndex=1\n");
    fprintf(fp, "dispKeyInSelList=0\n");
    fprintf(fp, "titleOrder=0\n");
    fprintf(fp, "omitHeader=0\n");
    fprintf(fp, "addKana=1\n");
    fprintf(fp, "autoKana=0\n");
    fprintf(fp, "withHeader=0\n");
    fprintf(fp, "optMono=0\n");
    fprintf(fp, "Size=20000;30000;100;3000000;20000;20000;20000;1000;1000;1000;1000\n");
    fprintf(fp, "Book=%s;%s;%s;_;", book_title, book_dir, book_type);
    fprintf(fp, "_;GAI16H00;GAI16F00;_;_;_;_;_;_;\n");
    fprintf(fp, "Source=$(BASE)\\%s;本文;_;HTML;\n", html_file);
    fclose(fp);
    message("終了しました\n");
    return OK;
}

/*
 * 設定ファイルを読み込む
 */
int
parse_ini_file(void)
{
    int n, lineno, ret;
    byte *p;
    byte key[MAX_WORD], val[MAX_PATH], buf[BUFSIZ];
    FILE *fp;

    if ((fp = fopen(INI_FILE, "r")) == NULL) {
	message("設定ファイル %s がオープンできません\n", INI_FILE);
	return ERR;
    }
    ret = OK;
    lineno = 0;
    while (fgets((char *)buf, BUFSIZ, fp) != NULL) {
	lineno++;
	p = skipsp(buf);
	if (*p == '#' || *p == '\r' || *p == '\n')
	    continue;
	p = getuptos(p, key, " \t=#");
	p = skipch(p, '=');
	p = skipsp(p);
	p = getuptos(p, val, "\t#");
	n = atoi(val);
	if (!strcmp(key, "BASEPATH")) {
	    if (p[-1] != '/' && p[-1] != '\\')
		strcat(val, "/");
	    base_path = strdup(val);
	} else if (!strcmp(key, "OUTPATH")) {
	    if (p[-1] != '/' && p[-1] != '\\')
		strcat(val, "/");
	    out_path = strdup(val);
	} else if (!strcmp(key, "SORTCMD")) {
	    sort_cmd = strdup(val);
	} else if (!strcmp(key, "AUTOKANA")) {
	    auto_kana = n;
	} else if (!strcmp(key, "EBTYPE")) {
	    eb_type = n;
	} else if (!strcmp(key, "BOOKTITLE")) {
	    book_title = strdup(val);
	    for (p = book_title; *p; p += 2) {
		if ((*p & 0x80) == 0)
		    break;
	    }
	    if (*p) {
		message("書籍タイトルに1バイト文字が含まれています(%d行め)\n", lineno);
		ret = ERR;
	    }
	} else if (!strcmp(key, "BOOKTYPE")) {
	    book_type = strdup(val);
	    if (strcmp(book_type, "国語辞典") &&
		strcmp(book_type, "漢和辞典") &&
		strcmp(book_type, "英和辞典") &&
		strcmp(book_type, "和英辞典") &&
		strcmp(book_type, "現代用語辞典") &&
		strcmp(book_type, "一般書物") &&
		strcmp(book_type, "類語辞典")) {
		message("未知の書籍種別が指定されています(%d行め)\n", lineno);
		ret = ERR;
	    }
	} else if (!strcmp(key, "BOOKDIR")) {
	    book_dir = strdup(val);
	    if (strlen(book_dir) > 8) {
		message("書籍ディレクトリ名が8バイトを超えています(%d行め)\n", lineno);
		ret = ERR;
	    }
	    for (p = book_dir; *p; p++) {
		if (*p >= 'A' && *p <= 'Z')
		    continue;
		if (*p >= '0' && *p <= '9' || *p == '_')
		    continue;
		break;
	    }
	    if (*p) {
		message("書籍ディレクトリ名に不正な文字(A-Z0-9_以外)が含まれています(%d行め)\n", lineno);
		ret = ERR;
	    }
	    if (ret != ERR) {
		sprintf(buf, "%s.html", book_dir);
		html_file = strdup(buf);
		sprintf(buf, "%s.ebs", book_dir);
		ebs_file = strdup(buf);
	    }
	} else {
	    message("設定ファイルに不正な行があります(%d行め)\n", lineno);
	    ret = ERR;
	}
    }
    fclose(fp);
    message("変換設定は以下のとおりです\n");
    message("  BASEPATH = %s\n", base_path);
    message("  OUTPATH = %s\n", out_path);
    message("  SORTCMD = %s\n", sort_cmd);
    message("  AUTOKANA = %d\n", auto_kana);
    message("  EBTYPE = %d\n", eb_type);
    message("  BOOKTITLE = %s\n", book_title);
    message("  BOOKTYPE = %s\n", book_type);
    message("  BOOKDIR = %s\n", book_dir);
    message("  生成されるHTMLファイル = %s\n", html_file);
    message("  生成されるEBSファイル = %s\n", ebs_file);
    return ret;
}

/*
 * ファイル出力先ディレクトリを作る
 */
int
work_directory(void)
{
    int i;
    char *p, path[MAX_PATH], subpath[MAX_PATH];
    struct stat st;

    if (*base_path &&
	strcmp(base_path, "\\") &&
	strcmp(base_path + 1, ":\\")) {
	strcpy(path, base_path);
	p = &path[strlen(path) - 1];
	if (*p == '/' || *p == '\\')
	    *p = '\0';
	if (stat(path, &st) < 0) {
	    if (mkdir(path,0755) < 0) {
		message("基準ディレクトリ %s が作れません\n", path);
		return ERR;
	    }
	    message("基準ディレクトリ %s を新規作成しました\n", path);
	} else if ((st.st_mode & S_IFMT) != S_IFDIR) {
	    message("基準ディレクトリ %s と同名のファイルがあります\n", path);
	    return ERR;
	}
    }
    if (*out_path &&
	strcmp(out_path, "\\") &&
	strcmp(out_path + 1, ":\\")) {
	strcpy(path, out_path);
	p = &path[strlen(path) - 1];
	if (*p == '/' || *p == '\\')
	    *p = '\0';
	if (stat(path, &st) < 0) {
	    if (mkdir(path,0755) < 0) {
		message("出力先ディレクトリ %s が作れません\n", path);
		return ERR;
	    }
	    message("出力先ディレクトリ %s を新規作成しました\n", path);
	} else if ((st.st_mode & S_IFMT) != S_IFDIR) {
	    message("出力先ディレクトリ %s と同名のファイルがあります\n", path);
	    return ERR;
	}
    }
    return OK;
}

/*
 * 利用可能なソートコマンドを設定する
 */
int
set_sort_command(void)
{
    char **p;
    struct stat st;
    static char *sort_cmd_cand[] = {
	"",
	"C:\\Windows\\System32\\sort.exe",
	"C:\\WinNT\\System32\\sort.exe",
	"C:\\Windows\\command\\sort.exe",
	NULL
    };

    if (sort_cmd && *sort_cmd)
	sort_cmd_cand[0] = sort_cmd;
    for (p = sort_cmd_cand; *p; p++) {
	if (stat(*p, &st) == 0) {
	    sort_cmd = *p;
	    return OK;
	}
    }
    message("ソートコマンドが見つかりません\n");
    return ERR;
}

/*
 * 初期化
 */
int
init(char *cmd_path)
{
    char *p, path[MAX_PATH];

    strcpy(path, cmd_path);
    p = &path[strlen(path) - 1];
    while (p > path && *p != '/')
	p--;
    *p = '\0';
    if (!strcmp(path + 1, ":"))
	strcat(path, "/");
    if (chdir(path) < 0) {
	printf("作業ディレクトリ %s への移動に失敗しました\n", path);
	return ERR;
    }
    time(&start_time);
    write_log("開始時刻: %s", ctime(&start_time));
    message("作業ディレクトリ %s に移動しました\n", path);
    return OK;
}

/*
 * 終了処理
 */
void
term(int status)
{
    time_t t;

    if (status == OK)
	message("変換処理が終了しました\n");
    else
	message("変換処理に失敗しました\n");
    time(&stop_time);
    t = stop_time - start_time;
    write_log("終了時刻: %s", ctime(&stop_time));
    message("経過時間: %d:%02d\n", (int)(t / 60), (int)(t % 60));
    if (status == OK) {
	message("※ %s%s を入力としてEBStudioを実行してください\n",
	    base_path, ebs_file);
    }
    write_log("\n");
    exit(status == ERR? 1: 0);
}

/*
 * メイン
 */
int
main(int ac, char **av)
{
    if (init(av[0]) == ERR)
	term(ERR);
    if (parse_ini_file() == ERR)
	term(ERR);
    if (work_directory() == ERR)
	term(ERR);
    if (set_sort_command() == ERR)
	term(ERR);
    if (generate_gaiji_file() == ERR)
	term(ERR);
    if (generate_work_file() == ERR)
	term(ERR);
    if (generate_html_file() == ERR)
	term(ERR);
    if (generate_ebs_file() == ERR)
	term(ERR);
    term(OK);
}
