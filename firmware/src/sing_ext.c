/* SPDX-License-Identifier: GPL-3.0-only
 * Compact transliteration readers. These are deliberately phonetic singing
 * approximations using VOICE's existing phonemes, not speech dictionaries.
 * Mandarin takes pinyin (notes determine pitch, not lexical tones). */
#define LR (1u<<(LG_RU-LG_RU))
#define LD (1u<<(LG_DE-LG_RU))
#define LF (1u<<(LG_FR-LG_RU))
#define LZ (1u<<(LG_ZH-LG_RU))
#define LP (1u<<(LG_PT-LG_RU))
#define LG (1u<<(LG_GN-LG_RU))
typedef struct { char s[5]; uint8_t langs,a,b; } sy_rule_t;
static const sy_rule_t SY_RULES[] LY_DICT_SEC = {
    {"shch",LR,PH_SH,0}, {"sch",LD,PH_SH,0}, {"eau",LF,PH_O,0}, {"oeu",LF,PH_ER,0},
    {"zh",LR|LZ,PH_ZH,0}, {"kh",LR,PH_X,0}, {"sh",LR|LZ,PH_SH,0},
    {"ch",LR|LZ,PH_CH,0}, {"ch",LD,PH_X,0}, {"ch",LF|LP|LG,PH_SH,0},
    {"ts",LR|LD,PH_TS,0}, {"yu",LR,PH_Y,PH_U}, {"ya",LR,PH_Y,PH_A},
    {"yo",LR,PH_Y,PH_O}, {"ye",LR,PH_Y,PH_E},
    {"ei",LD,PH_A,PH_I}, {"ie",LD,PH_I,0}, {"eu",LD,PH_O,PH_I},
    {"ae",LD,PH_EH,0}, {"oe",LD,PH_ER,0}, {"ue",LD,PH_UJ,0},
    {"ei",LF,PH_E,0}, {"eh",LF,PH_EH,0},
    {"ou",LF,PH_U,0}, {"eu",LF,PH_ER,0}, {"oi",LF,PH_W,PH_A},
    {"gn",LF,PH_NY,0}, {"nh",LP,PH_NY,0}, {"lh",LP,PH_Y,0},
    {"ny",LG,PH_NY,0}, {"ng",LZ|LG,PH_NG,0},
    {"a~",LF|LP|LG,PH_A,PH_NM}, {"e~",LF|LP|LG,PH_E,PH_NM},
    {"i~",LF|LP|LG,PH_I,PH_NM}, {"o~",LF|LP|LG,PH_O,PH_NM},
    {"u~",LF|LP|LG,PH_U,PH_NM}, {"y~",LG,PH_UH,PH_NM},
    {"j",LD,PH_Y,0}, {"w",LD,PH_V,0}, {"v",LD,PH_F,0}, {"z",LD,PH_TS,0},
    {"j",LF|LP|LG,PH_ZH,0}, {"r",LF,PH_X,0},
    {"x",LP|LZ,PH_SH,0}, {"q",LZ,PH_CH,0}, {"j",LZ,PH_J,0},
    {"c",LZ,PH_TS,0}, {"z",LZ,PH_TS,0}, {"r",LZ,PH_RA,0},
    {"v",LZ,PH_UJ,0}, {"y",LR|LG,PH_UH,0},
    {"h",LR|LD|LZ|LP|LG,PH_H,0}, {"v",LR|LF|LP,PH_V,0},
    {"g",LR|LD|LZ|LG,PH_G,0}, {"j",LR,PH_Y,0}, {"z",LR|LF|LP,PH_Z,0},
    {"'",LG,PH_H,0}
};
static uint32_t sy_ext(uint32_t lang, const char *w, uint32_t n, uint8_t *tc, uint8_t *tk)
{
    if(!ly_en_ok) return sy_es_tok(w,n,tc,tk); /* unverified/missing optional bank */
    uint32_t i=0,m=0,mask=1u<<(lang-LG_RU);
    while(i<n && m+2u<=SW_MAX) {
        uint32_t j,k=0,a=0,b=0;
        /* French unaccented final e and common final silent consonants. */
        if(lang==LG_FR && i+1u==n && i &&
           (w[i]=='e'||w[i]=='s'||w[i]=='t'||w[i]=='d'||w[i]=='x')) break;
        for(j=0;j<NELEM(SY_RULES);j++) {
            const sy_rule_t *r=&SY_RULES[j];
            if(!(r->langs&mask)) continue;
            for(k=0;r->s[k] && i+k<n && w[i+k]==r->s[k];k++);
            if(!r->s[k]) { a=r->a;b=r->b;break; }
        }
        if(j==NELEM(SY_RULES)) {
            static const uint8_t letters[26]={PH_A,PH_B,PH_K,PH_D,PH_E,PH_F,PH_G,0,PH_I,PH_X,
                PH_K,PH_L,PH_M,PH_N,PH_O,PH_P,PH_K,PH_RT,PH_S,PH_T,PH_U,PH_V,PH_W,PH_S,PH_Y,PH_Z};
            k=1;
            if(w[i]>='a'&&w[i]<='z') a=letters[(uint32_t)(w[i]-'a')];
            if((lang==LG_FR||lang==LG_PT) && i+1u<n && (w[i+1u]=='e'||w[i+1u]=='i')) {
                if(w[i]=='c') a=PH_S;
                if(w[i]=='g') a=PH_ZH;
            }
        }
        i+=k;
        if(a) { tc[m]=(uint8_t)a;tk[m++]=ph_vowel(a)?((a==PH_I||a==PH_U)?TK_W:TK_S):TK_C; }
        if(b) { tc[m]=(uint8_t)b;tk[m++]=ph_vowel(b)?TK_W:TK_C; }
    }
    return m;
}
/* Two original seed words per theme; free TEXT is the intentional lyric mode.
 * No copied dictionaries, extra cache or phoneme inventory. */
static const char SY_EXT_WORD_0[] LY_DICT_SEC = "lyubov\0serdtse\0noch\0luna\0more\0volna\0nebo\0zvezda\0mir\0pesnya\0doroga\0veter\0son\0mechta\0ogon\0iskra";
static const char SY_EXT_WORD_1[] LY_DICT_SEC = "liebe\0herz\0nacht\0mond\0meer\0welle\0himmel\0stern\0welt\0lied\0weg\0wind\0traum\0schlaf\0feuer\0licht";
static const char SY_EXT_WORD_2[] LY_DICT_SEC = "amour\0coeur\0nuit\0lune\0mer\0vague\0ciel\0soleil\0monde\0chanson\0route\0vent\0reve\0dormir\0feu\0flamme";
static const char SY_EXT_WORD_3[] LY_DICT_SEC = "ai\0xin\0ye\0yue\0hai\0lang\0tian\0xing\0ren\0ge\0lu\0feng\0meng\0shui\0huo\0guang";
static const char SY_EXT_WORD_4[] LY_DICT_SEC = "amor\0corasa~o\0noite\0lua\0mar\0onda\0ceu\0estrela\0mundo\0canto\0caminho\0vento\0sonho\0dormir\0fogo\0luz";
static const char SY_EXT_WORD_5[] LY_DICT_SEC = "mborayhu\0coraso~\0pyhare\0jasy\0para\0y\0yvaga\0mbyja\0ava\0purahei\0tape\0yvytu\0kerana\0ke\0tata\0tesape";
static const char *const SY_EXT_WORDS[] = {SY_EXT_WORD_0, SY_EXT_WORD_1, SY_EXT_WORD_2, SY_EXT_WORD_3, SY_EXT_WORD_4, SY_EXT_WORD_5};
#undef LR
#undef LD
#undef LF
#undef LZ
#undef LP
#undef LG
