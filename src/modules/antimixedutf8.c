/*
 * Anti mixed UTF8 - a filter written by Bram Matthys ("Syzop").
 * Reported by Mr_Smoke in https://bugs.unrealircd.org/view.php?id=5163
 * Tested by PeGaSuS (The_Myth) with some of the most used spam lines.
 * Help with testing and fixing Cyrillic from 'i' <info@servx.org>
 *
 * ==[ ABOUT ]==
 * This module will detect and stop spam containing of characters of
 * mixed "scripts", where some characters are in Latin script and other
 * characters are in Cyrillic.
 * This unusual behavior can be detected easily and action can be taken.
 *
 * ==[ MODULE LOADING AND CONFIGURATION ]==
 * loadmodule "antimixedutf8";
 * set {
 *         antimixedutf8 {
 *                 score 10;
 *                 ban-action block;
 *                 ban-reason "Possible mixed character spam";
 *                 ban-time 4h; // For other types
 *                 except {
 *                 }
 *         };
 * };
 *
 * ==[ LICENSE AND PORTING ]==
 * Feel free to copy/move the idea or code to other IRCds.
 * The license is GPLv1 (or later, at your option):
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* The detection algorithm follows first, the module/config code is at the end. */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"antimixedutf8",
	"2.0",
	"Mixed UTF8 character filter (look-alike character spam) - by Syzop",
	"UnrealIRCd Team",
	"unrealircd-6",
};

struct {
	int score;
	BanAction *ban_action;
	char *ban_reason;
	long ban_time;
	SecurityGroup *except;
} cfg;

static void free_config(void);
static void init_config(void);
int antimixedutf8_config_test(ConfigFile *, ConfigEntry *, int, int *);
int antimixedutf8_config_run(ConfigFile *, ConfigEntry *, int);

/* https://unicode.org/Public/UNIDATA/Blocks.txt */

typedef struct UnicodeBlocks {
	uint32_t start;
	uint32_t end;
	const char *name;
	int score;
} UnicodeBlocks;

/* This is the list of all the unicode blocks.
 * If you want to ignore transition to/from a block, then
 * you can comment it out by putting // in front,
 * transitions to/from that code block will then not lead to a score.
 */

UnicodeBlocks unicode_blocks[] =
{
	{0x0000, 0x007F, "Basic Latin", 1},
	{0x0080, 0x00FF, "Latin-1 Supplement", 1},
	{0x0100, 0x017F, "Latin Extended-A", 1},
	{0x0180, 0x024F, "Latin Extended-B", 1},
	{0x0250, 0x02AF, "IPA Extensions", 1},
	{0x02B0, 0x02FF, "Spacing Modifier Letters", 1},
	{0x0300, 0x036F, "Combining Diacritical Marks", 1},
	{0x0370, 0x03FF, "Greek and Coptic", 1},
	{0x0400, 0x04FF, "Cyrillic", 1},
	{0x0500, 0x052F, "Cyrillic Supplement", 1},
	{0x0530, 0x058F, "Armenian", 1},
	{0x0590, 0x05FF, "Hebrew", 1},
	{0x0600, 0x06FF, "Arabic", 1},
	{0x0700, 0x074F, "Syriac", 1},
	{0x0750, 0x077F, "Arabic Supplement", 1},
	{0x0780, 0x07BF, "Thaana", 1},
	{0x07C0, 0x07FF, "NKo", 1},
	{0x0800, 0x083F, "Samaritan", 1},
	{0x0840, 0x085F, "Mandaic", 1},
	{0x0860, 0x086F, "Syriac Supplement", 1},
	{0x0870, 0x089F, "Arabic Extended-B", 1},
	{0x08A0, 0x08FF, "Arabic Extended-A", 1},
	{0x0900, 0x097F, "Devanagari", 1},
	{0x0980, 0x09FF, "Bengali", 1},
	{0x0A00, 0x0A7F, "Gurmukhi", 1},
	{0x0A80, 0x0AFF, "Gujarati", 1},
	{0x0B00, 0x0B7F, "Oriya", 1},
	{0x0B80, 0x0BFF, "Tamil", 1},
	{0x0C00, 0x0C7F, "Telugu", 1},
	{0x0C80, 0x0CFF, "Kannada", 1},
	{0x0D00, 0x0D7F, "Malayalam", 1},
	{0x0D80, 0x0DFF, "Sinhala", 1},
	{0x0E00, 0x0E7F, "Thai", 1},
	{0x0E80, 0x0EFF, "Lao", 1},
	{0x0F00, 0x0FFF, "Tibetan", 1},
	{0x1000, 0x109F, "Myanmar", 1},
	{0x10A0, 0x10FF, "Georgian", 1},
	{0x1100, 0x11FF, "Hangul Jamo", 1},
	{0x1200, 0x137F, "Ethiopic", 1},
	{0x1380, 0x139F, "Ethiopic Supplement", 1},
	{0x13A0, 0x13FF, "Cherokee", 1},
	{0x1400, 0x167F, "Unified Canadian Aboriginal Syllabics", 1},
	{0x1680, 0x169F, "Ogham", 1},
	{0x16A0, 0x16FF, "Runic", 1},
	{0x1700, 0x171F, "Tagalog", 1},
	{0x1720, 0x173F, "Hanunoo", 1},
	{0x1740, 0x175F, "Buhid", 1},
	{0x1760, 0x177F, "Tagbanwa", 1},
	{0x1780, 0x17FF, "Khmer", 1},
	{0x1800, 0x18AF, "Mongolian", 1},
	{0x18B0, 0x18FF, "Unified Canadian Aboriginal Syllabics Extended", 1},
	{0x1900, 0x194F, "Limbu", 1},
	{0x1950, 0x197F, "Tai Le", 1},
	{0x1980, 0x19DF, "New Tai Lue", 1},
	{0x19E0, 0x19FF, "Khmer Symbols", 1},
	{0x1A00, 0x1A1F, "Buginese", 1},
	{0x1A20, 0x1AAF, "Tai Tham", 1},
	{0x1AB0, 0x1AFF, "Combining Diacritical Marks Extended", 1},
	{0x1B00, 0x1B7F, "Balinese", 1},
	{0x1B80, 0x1BBF, "Sundanese", 1},
	{0x1BC0, 0x1BFF, "Batak", 1},
	{0x1C00, 0x1C4F, "Lepcha", 1},
	{0x1C50, 0x1C7F, "Ol Chiki", 1},
	{0x1C80, 0x1C8F, "Cyrillic Extended-C", 1},
	{0x1C90, 0x1CBF, "Georgian Extended", 1},
	{0x1CC0, 0x1CCF, "Sundanese Supplement", 1},
	{0x1CD0, 0x1CFF, "Vedic Extensions", 1},
	{0x1D00, 0x1D7F, "Phonetic Extensions", 1},
	{0x1D80, 0x1DBF, "Phonetic Extensions Supplement", 1},
	{0x1DC0, 0x1DFF, "Combining Diacritical Marks Supplement", 1},
	{0x1E00, 0x1EFF, "Latin Extended Additional", 1},
	{0x1F00, 0x1FFF, "Greek Extended", 1},
//	{0x2000, 0x206F, "General Punctuation", 1},
	{0x2070, 0x209F, "Superscripts and Subscripts", 1},
	{0x20A0, 0x20CF, "Currency Symbols", 1},
	{0x20D0, 0x20FF, "Combining Diacritical Marks for Symbols", 1},
	{0x2100, 0x214F, "Letterlike Symbols", 1},
	{0x2150, 0x218F, "Number Forms", 1},
	{0x2190, 0x21FF, "Arrows", 1},
	{0x2200, 0x22FF, "Mathematical Operators", 1},
	{0x2300, 0x23FF, "Miscellaneous Technical", 1},
	{0x2400, 0x243F, "Control Pictures", 1},
	{0x2440, 0x245F, "Optical Character Recognition", 1},
	{0x2460, 0x24FF, "Enclosed Alphanumerics", 1},
	{0x2500, 0x257F, "Box Drawing", 1},
	{0x2580, 0x259F, "Block Elements", 1},
	{0x25A0, 0x25FF, "Geometric Shapes", 1},
	{0x2600, 0x26FF, "Miscellaneous Symbols", 1},
	{0x2700, 0x27BF, "Dingbats", 1},
	{0x27C0, 0x27EF, "Miscellaneous Mathematical Symbols-A", 1},
	{0x27F0, 0x27FF, "Supplemental Arrows-A", 1},
	{0x2800, 0x28FF, "Braille Patterns", 1},
	{0x2900, 0x297F, "Supplemental Arrows-B", 1},
	{0x2980, 0x29FF, "Miscellaneous Mathematical Symbols-B", 1},
	{0x2A00, 0x2AFF, "Supplemental Mathematical Operators", 1},
	{0x2B00, 0x2BFF, "Miscellaneous Symbols and Arrows", 1},
	{0x2C00, 0x2C5F, "Glagolitic", 1},
	{0x2C60, 0x2C7F, "Latin Extended-C", 1},
	{0x2C80, 0x2CFF, "Coptic", 1},
	{0x2D00, 0x2D2F, "Georgian Supplement", 1},
	{0x2D30, 0x2D7F, "Tifinagh", 1},
	{0x2D80, 0x2DDF, "Ethiopic Extended", 1},
	{0x2DE0, 0x2DFF, "Cyrillic Extended-A", 1},
	{0x2E00, 0x2E7F, "Supplemental Punctuation", 1},
	{0x2E80, 0x2EFF, "CJK Radicals Supplement", 1},
	{0x2F00, 0x2FDF, "Kangxi Radicals", 1},
	{0x2FF0, 0x2FFF, "Ideographic Description Characters", 1},
	{0x3000, 0x303F, "CJK Symbols and Punctuation", 1},
	{0x3040, 0x309F, "Hiragana", 1},
	{0x30A0, 0x30FF, "Katakana", 1},
	{0x3100, 0x312F, "Bopomofo", 1},
	{0x3130, 0x318F, "Hangul Compatibility Jamo", 1},
	{0x3190, 0x319F, "Kanbun", 1},
	{0x31A0, 0x31BF, "Bopomofo Extended", 1},
	{0x31C0, 0x31EF, "CJK Strokes", 1},
	{0x31F0, 0x31FF, "Katakana Phonetic Extensions", 1},
	{0x3200, 0x32FF, "Enclosed CJK Letters and Months", 1},
	{0x3300, 0x33FF, "CJK Compatibility", 1},
	{0x3400, 0x4DBF, "CJK Unified Ideographs Extension A", 1},
	{0x4DC0, 0x4DFF, "Yijing Hexagram Symbols", 1},
	{0x4E00, 0x9FFF, "CJK Unified Ideographs", 1},
	{0xA000, 0xA48F, "Yi Syllables", 1},
	{0xA490, 0xA4CF, "Yi Radicals", 1},
	{0xA4D0, 0xA4FF, "Lisu", 1},
	{0xA500, 0xA63F, "Vai", 1},
	{0xA640, 0xA69F, "Cyrillic Extended-B", 1},
	{0xA6A0, 0xA6FF, "Bamum", 1},
	{0xA700, 0xA71F, "Modifier Tone Letters", 1},
	{0xA720, 0xA7FF, "Latin Extended-D", 1},
	{0xA800, 0xA82F, "Syloti Nagri", 1},
	{0xA830, 0xA83F, "Common Indic Number Forms", 1},
	{0xA840, 0xA87F, "Phags-pa", 1},
	{0xA880, 0xA8DF, "Saurashtra", 1},
	{0xA8E0, 0xA8FF, "Devanagari Extended", 1},
	{0xA900, 0xA92F, "Kayah Li", 1},
	{0xA930, 0xA95F, "Rejang", 1},
	{0xA960, 0xA97F, "Hangul Jamo Extended-A", 1},
	{0xA980, 0xA9DF, "Javanese", 1},
	{0xA9E0, 0xA9FF, "Myanmar Extended-B", 1},
	{0xAA00, 0xAA5F, "Cham", 1},
	{0xAA60, 0xAA7F, "Myanmar Extended-A", 1},
	{0xAA80, 0xAADF, "Tai Viet", 1},
	{0xAAE0, 0xAAFF, "Meetei Mayek Extensions", 1},
	{0xAB00, 0xAB2F, "Ethiopic Extended-A", 1},
	{0xAB30, 0xAB6F, "Latin Extended-E", 1},
	{0xAB70, 0xABBF, "Cherokee Supplement", 1},
	{0xABC0, 0xABFF, "Meetei Mayek", 1},
	{0xAC00, 0xD7AF, "Hangul Syllables", 1},
	{0xD7B0, 0xD7FF, "Hangul Jamo Extended-B", 1},
	{0xD800, 0xDB7F, "High Surrogates", 1},
	{0xDB80, 0xDBFF, "High Private Use Surrogates", 1},
	{0xDC00, 0xDFFF, "Low Surrogates", 1},
	{0xE000, 0xF8FF, "Private Use Area", 1},
	{0xF900, 0xFAFF, "CJK Compatibility Ideographs", 1},
	{0xFB00, 0xFB4F, "Alphabetic Presentation Forms", 1},
	{0xFB50, 0xFDFF, "Arabic Presentation Forms-A", 1},
	{0xFE00, 0xFE0F, "Variation Selectors", 1},
	{0xFE10, 0xFE1F, "Vertical Forms", 1},
	{0xFE20, 0xFE2F, "Combining Half Marks", 1},
	{0xFE30, 0xFE4F, "CJK Compatibility Forms", 1},
	{0xFE50, 0xFE6F, "Small Form Variants", 1},
	{0xFE70, 0xFEFF, "Arabic Presentation Forms-B", 1},
	{0xFF00, 0xFFEF, "Halfwidth and Fullwidth Forms", 1},
	{0xFFF0, 0xFFFF, "Specials", 1},
	{0x10000, 0x1007F, "Linear B Syllabary", 1},
	{0x10080, 0x100FF, "Linear B Ideograms", 1},
	{0x10100, 0x1013F, "Aegean Numbers", 1},
	{0x10140, 0x1018F, "Ancient Greek Numbers", 1},
	{0x10190, 0x101CF, "Ancient Symbols", 1},
	{0x101D0, 0x101FF, "Phaistos Disc", 1},
	{0x10280, 0x1029F, "Lycian", 1},
	{0x102A0, 0x102DF, "Carian", 1},
	{0x102E0, 0x102FF, "Coptic Epact Numbers", 1},
	{0x10300, 0x1032F, "Old Italic", 1},
	{0x10330, 0x1034F, "Gothic", 1},
	{0x10350, 0x1037F, "Old Permic", 1},
	{0x10380, 0x1039F, "Ugaritic", 1},
	{0x103A0, 0x103DF, "Old Persian", 1},
	{0x10400, 0x1044F, "Deseret", 1},
	{0x10450, 0x1047F, "Shavian", 1},
	{0x10480, 0x104AF, "Osmanya", 1},
	{0x104B0, 0x104FF, "Osage", 1},
	{0x10500, 0x1052F, "Elbasan", 1},
	{0x10530, 0x1056F, "Caucasian Albanian", 1},
	{0x10570, 0x105BF, "Vithkuqi", 1},
	{0x10600, 0x1077F, "Linear A", 1},
	{0x10780, 0x107BF, "Latin Extended-F", 1},
	{0x10800, 0x1083F, "Cypriot Syllabary", 1},
	{0x10840, 0x1085F, "Imperial Aramaic", 1},
	{0x10860, 0x1087F, "Palmyrene", 1},
	{0x10880, 0x108AF, "Nabataean", 1},
	{0x108E0, 0x108FF, "Hatran", 1},
	{0x10900, 0x1091F, "Phoenician", 1},
	{0x10920, 0x1093F, "Lydian", 1},
	{0x10980, 0x1099F, "Meroitic Hieroglyphs", 1},
	{0x109A0, 0x109FF, "Meroitic Cursive", 1},
	{0x10A00, 0x10A5F, "Kharoshthi", 1},
	{0x10A60, 0x10A7F, "Old South Arabian", 1},
	{0x10A80, 0x10A9F, "Old North Arabian", 1},
	{0x10AC0, 0x10AFF, "Manichaean", 1},
	{0x10B00, 0x10B3F, "Avestan", 1},
	{0x10B40, 0x10B5F, "Inscriptional Parthian", 1},
	{0x10B60, 0x10B7F, "Inscriptional Pahlavi", 1},
	{0x10B80, 0x10BAF, "Psalter Pahlavi", 1},
	{0x10C00, 0x10C4F, "Old Turkic", 1},
	{0x10C80, 0x10CFF, "Old Hungarian", 1},
	{0x10D00, 0x10D3F, "Hanifi Rohingya", 1},
	{0x10E60, 0x10E7F, "Rumi Numeral Symbols", 1},
	{0x10E80, 0x10EBF, "Yezidi", 1},
	{0x10EC0, 0x10EFF, "Arabic Extended-C", 1},
	{0x10F00, 0x10F2F, "Old Sogdian", 1},
	{0x10F30, 0x10F6F, "Sogdian", 1},
	{0x10F70, 0x10FAF, "Old Uyghur", 1},
	{0x10FB0, 0x10FDF, "Chorasmian", 1},
	{0x10FE0, 0x10FFF, "Elymaic", 1},
	{0x11000, 0x1107F, "Brahmi", 1},
	{0x11080, 0x110CF, "Kaithi", 1},
	{0x110D0, 0x110FF, "Sora Sompeng", 1},
	{0x11100, 0x1114F, "Chakma", 1},
	{0x11150, 0x1117F, "Mahajani", 1},
	{0x11180, 0x111DF, "Sharada", 1},
	{0x111E0, 0x111FF, "Sinhala Archaic Numbers", 1},
	{0x11200, 0x1124F, "Khojki", 1},
	{0x11280, 0x112AF, "Multani", 1},
	{0x112B0, 0x112FF, "Khudawadi", 1},
	{0x11300, 0x1137F, "Grantha", 1},
	{0x11400, 0x1147F, "Newa", 1},
	{0x11480, 0x114DF, "Tirhuta", 1},
	{0x11580, 0x115FF, "Siddham", 1},
	{0x11600, 0x1165F, "Modi", 1},
	{0x11660, 0x1167F, "Mongolian Supplement", 1},
	{0x11680, 0x116CF, "Takri", 1},
	{0x11700, 0x1174F, "Ahom", 1},
	{0x11800, 0x1184F, "Dogra", 1},
	{0x118A0, 0x118FF, "Warang Citi", 1},
	{0x11900, 0x1195F, "Dives Akuru", 1},
	{0x119A0, 0x119FF, "Nandinagari", 1},
	{0x11A00, 0x11A4F, "Zanabazar Square", 1},
	{0x11A50, 0x11AAF, "Soyombo", 1},
	{0x11AB0, 0x11ABF, "Unified Canadian Aboriginal Syllabics Extended-A", 1},
	{0x11AC0, 0x11AFF, "Pau Cin Hau", 1},
	{0x11B00, 0x11B5F, "Devanagari Extended-A", 1},
	{0x11C00, 0x11C6F, "Bhaiksuki", 1},
	{0x11C70, 0x11CBF, "Marchen", 1},
	{0x11D00, 0x11D5F, "Masaram Gondi", 1},
	{0x11D60, 0x11DAF, "Gunjala Gondi", 1},
	{0x11EE0, 0x11EFF, "Makasar", 1},
	{0x11F00, 0x11F5F, "Kawi", 1},
	{0x11FB0, 0x11FBF, "Lisu Supplement", 1},
	{0x11FC0, 0x11FFF, "Tamil Supplement", 1},
	{0x12000, 0x123FF, "Cuneiform", 1},
	{0x12400, 0x1247F, "Cuneiform Numbers and Punctuation", 1},
	{0x12480, 0x1254F, "Early Dynastic Cuneiform", 1},
	{0x12F90, 0x12FFF, "Cypro-Minoan", 1},
	{0x13000, 0x1342F, "Egyptian Hieroglyphs", 1},
	{0x13430, 0x1345F, "Egyptian Hieroglyph Format Controls", 1},
	{0x14400, 0x1467F, "Anatolian Hieroglyphs", 1},
	{0x16800, 0x16A3F, "Bamum Supplement", 1},
	{0x16A40, 0x16A6F, "Mro", 1},
	{0x16A70, 0x16ACF, "Tangsa", 1},
	{0x16AD0, 0x16AFF, "Bassa Vah", 1},
	{0x16B00, 0x16B8F, "Pahawh Hmong", 1},
	{0x16E40, 0x16E9F, "Medefaidrin", 1},
	{0x16F00, 0x16F9F, "Miao", 1},
	{0x16FE0, 0x16FFF, "Ideographic Symbols and Punctuation", 1},
	{0x17000, 0x187FF, "Tangut", 1},
	{0x18800, 0x18AFF, "Tangut Components", 1},
	{0x18B00, 0x18CFF, "Khitan Small Script", 1},
	{0x18D00, 0x18D7F, "Tangut Supplement", 1},
	{0x1AFF0, 0x1AFFF, "Kana Extended-B", 1},
	{0x1B000, 0x1B0FF, "Kana Supplement", 1},
	{0x1B100, 0x1B12F, "Kana Extended-A", 1},
	{0x1B130, 0x1B16F, "Small Kana Extension", 1},
	{0x1B170, 0x1B2FF, "Nushu", 1},
	{0x1BC00, 0x1BC9F, "Duployan", 1},
	{0x1BCA0, 0x1BCAF, "Shorthand Format Controls", 1},
	{0x1CF00, 0x1CFCF, "Znamenny Musical Notation", 1},
	{0x1D000, 0x1D0FF, "Byzantine Musical Symbols", 1},
	{0x1D100, 0x1D1FF, "Musical Symbols", 1},
	{0x1D200, 0x1D24F, "Ancient Greek Musical Notation", 1},
	{0x1D2C0, 0x1D2DF, "Kaktovik Numerals", 1},
	{0x1D2E0, 0x1D2FF, "Mayan Numerals", 1},
	{0x1D300, 0x1D35F, "Tai Xuan Jing Symbols", 1},
	{0x1D360, 0x1D37F, "Counting Rod Numerals", 1},
	{0x1D400, 0x1D7FF, "Mathematical Alphanumeric Symbols", 3},
	{0x1D800, 0x1DAAF, "Sutton SignWriting", 1},
	{0x1DF00, 0x1DFFF, "Latin Extended-G", 1},
	{0x1E000, 0x1E02F, "Glagolitic Supplement", 1},
	{0x1E030, 0x1E08F, "Cyrillic Extended-D", 1},
	{0x1E100, 0x1E14F, "Nyiakeng Puachue Hmong", 1},
	{0x1E290, 0x1E2BF, "Toto", 1},
	{0x1E2C0, 0x1E2FF, "Wancho", 1},
	{0x1E4D0, 0x1E4FF, "Nag Mundari", 1},
	{0x1E7E0, 0x1E7FF, "Ethiopic Extended-B", 1},
	{0x1E800, 0x1E8DF, "Mende Kikakui", 1},
	{0x1E900, 0x1E95F, "Adlam", 1},
	{0x1EC70, 0x1ECBF, "Indic Siyaq Numbers", 1},
	{0x1ED00, 0x1ED4F, "Ottoman Siyaq Numbers", 1},
	{0x1EE00, 0x1EEFF, "Arabic Mathematical Alphabetic Symbols", 1},
	{0x1F000, 0x1F02F, "Mahjong Tiles", 1},
	{0x1F030, 0x1F09F, "Domino Tiles", 1},
	{0x1F0A0, 0x1F0FF, "Playing Cards", 1},
	{0x1F100, 0x1F1FF, "Enclosed Alphanumeric Supplement", 1},
	{0x1F200, 0x1F2FF, "Enclosed Ideographic Supplement", 1},
	{0x1F300, 0x1F5FF, "Miscellaneous Symbols and Pictographs", 1},
	{0x1F600, 0x1F64F, "Emoticons", 1},
	{0x1F650, 0x1F67F, "Ornamental Dingbats", 1},
	{0x1F680, 0x1F6FF, "Transport and Map Symbols", 1},
	{0x1F700, 0x1F77F, "Alchemical Symbols", 1},
	{0x1F780, 0x1F7FF, "Geometric Shapes Extended", 1},
	{0x1F800, 0x1F8FF, "Supplemental Arrows-C", 1},
	{0x1F900, 0x1F9FF, "Supplemental Symbols and Pictographs", 1},
	{0x1FA00, 0x1FA6F, "Chess Symbols", 1},
	{0x1FA70, 0x1FAFF, "Symbols and Pictographs Extended-A", 1},
	{0x1FB00, 0x1FBFF, "Symbols for Legacy Computing", 1},
	{0x20000, 0x2A6DF, "CJK Unified Ideographs Extension B", 1},
	{0x2A700, 0x2B73F, "CJK Unified Ideographs Extension C", 1},
	{0x2B740, 0x2B81F, "CJK Unified Ideographs Extension D", 1},
	{0x2B820, 0x2CEAF, "CJK Unified Ideographs Extension E", 1},
	{0x2CEB0, 0x2EBEF, "CJK Unified Ideographs Extension F", 1},
	{0x2F800, 0x2FA1F, "CJK Compatibility Ideographs Supplement", 1},
	{0x30000, 0x3134F, "CJK Unified Ideographs Extension G", 1},
	{0x31350, 0x323AF, "CJK Unified Ideographs Extension H", 1},
	{0xE0000, 0xE007F, "Tags", 1},
	{0xE0100, 0xE01EF, "Variation Selectors Supplement", 1},
	{0xF0000, 0xFFFFF, "Supplementary Private Use Area-A", 1},
	{0x100000, 0x10FFFF, "Supplementary Private Use Area-B", 1}
};

#define IS_IN_RANGE(c, f, l)    (((c) >= (f)) && ((c) <= (l)))

#define SCRIPT_UNDEFINED -1

uint32_t utf8_to_utf32(const char *t)
{
	char c1, c2;
	const char *ptr = t;
	uint32_t uc = 0;
	int i;
	char seqlen = 0;

	c1 = ptr[0];
	if( (c1 & 0x80) == 0 )
	{
		uc = (u_long) (c1 & 0x7F);
		seqlen = 1;
	}
	else if( (c1 & 0xE0) == 0xC0 )
	{
		uc = (u_long) (c1 & 0x1F);
		seqlen = 2;
	}
	else if( (c1 & 0xF0) == 0xE0 )
	{
		uc = (u_long) (c1 & 0x0F);
		seqlen = 3;
	}
	else if( (c1 & 0xF8) == 0xF0 )
	{
		uc = (u_long) (c1 & 0x07);
		seqlen = 4;
	} else
		return -1; /* should be impossible */

	for (i = 1; i < seqlen; ++i)
	{
		c1 = ptr[i];

		if( (c1 & 0xC0) != 0x80 )
		{
			// malformed data, do something !!!
			return (uint32_t) -1;
		}
	}

	switch( seqlen )
	{
		case 2:
		{
			c1 = ptr[0];

			if( !IS_IN_RANGE(c1, 0xC2, 0xDF) )
			{
				// malformed data, do something !!!
				return (uint32_t) -1;
			}

			break;
		}

		case 3:
		{
			c1 = ptr[0];
			c2 = ptr[1];

			switch (c1)
			{
				case 0xE0:
					if (!IS_IN_RANGE(c2, 0xA0, 0xBF))
					{
						// malformed data, do something !!!
						return (uint32_t) -1;
					}
					break;

				case 0xED:
					if (!IS_IN_RANGE(c2, 0x80, 0x9F))
					{
						// malformed data, do something !!!
						return (uint32_t) -1;
					}
					break;

				default:
					if (!IS_IN_RANGE(c1, 0xE1, 0xEC) && !IS_IN_RANGE(c1, 0xEE, 0xEF))
					{
						// malformed data, do something !!!
						return (uint32_t) -1;
					}
					break;
			}

			break;
		}

		case 4:
		{
			c1 = ptr[0];
			c2 = ptr[1];

			switch (c1)
			{
				case 0xF0:
					if (!IS_IN_RANGE(c2, 0x90, 0xBF))
					{
						// malformed data, do something !!!
						return (uint32_t) -1;
					}
					break;

				case 0xF4:
					if (!IS_IN_RANGE(c2, 0x80, 0x8F))
					{
						// malformed data, do something !!!
						return (uint32_t) -1;
					}
					break;

				default:
					if (!IS_IN_RANGE(c1, 0xF1, 0xF3))
					{
						// malformed data, do something !!!
						return (uint32_t) -1;
					}
					break;
			}

			break;
		}
	}

	for (i = 1; i < seqlen; ++i)
	{
		uc = ((uc << 6) | (uint32_t)(ptr[i] & 0x3F));
	}
	return uc;
}

/** Detect which script the current character is,
 * such as latin script or cyrillic script.
 * @retval See SCRIPT_*
 */
int detect_script(uint32_t utfchar)
{
	int i;

	/* Special handling for ASCII:
	 * - Consider only a-z/A-Z as latin1
	 * - All the rest, like spaces or numbers and stuff is
	 *   seen as SCRIPT_UNDEFINED, since it can and will
	 *   likely be used a lot with other unicode blocks too.
	 */
	if (utfchar <= 127)
	{
		if (((utfchar >= 'a') && (utfchar <= 'z')) ||
		    ((utfchar >= 'A') && (utfchar <= 'Z')))
		{
			return 0;
		}
		return SCRIPT_UNDEFINED;
	}

	for (i=0; i < elementsof(unicode_blocks); i++)
	{
		if ((utfchar >= unicode_blocks[i].start) &&
		    (utfchar <= unicode_blocks[i].end))
		{
			return i;
		}
	}

	return SCRIPT_UNDEFINED;
}

/** Returns length of an (UTF8) character. May return <1 for error conditions.
 * Made by i <info@servx.org>
 */
static int utf8_charlen(const char *str)
{
	struct { char mask; char val; } t[4] =
	{ { 0x80, 0x00 }, { 0xE0, 0xC0 }, { 0xF0, 0xE0 }, { 0xF8, 0xF0 } };
	unsigned k, j;

	for (k = 0; k < 4; k++)
	{
		if ((*str & t[k].mask) == t[k].val)
		{
			for (j = 0; j < k; j++)
			{
				if ((*(++str) & 0xC0) != 0x80)
					return -1;
			}
			return k + 1;
		}
	}
	return 1;
}

int lookalikespam_score(const char *text, char *logbuf, size_t logbufsize)
{
	const char *p;
	int last_script = 0;
	int current_script;
	int points = 0;
	int last_character_was_word_separator = 0;
	int skip;
	u_int32_t utfchar;

	for (p = text; *p; p++)
	{
		utfchar = utf8_to_utf32(p);
		current_script = detect_script(utfchar);

		if (current_script != SCRIPT_UNDEFINED)
		{
			if ((current_script != last_script) && (last_script != SCRIPT_UNDEFINED))
			{
				int add_points = unicode_blocks[current_script].score;
				/* A script change = 1 point */
				points += add_points;

				/* Double the points if the script change happened
				 * within the same word, as that would be rather unusual
				 * in normal cases.
				 */
				if (!last_character_was_word_separator)
					points += add_points;

				if (logbuf)
				{
					strlcat(logbuf, unicode_blocks[current_script].name, logbufsize);
					strlcat(logbuf, ", ", logbufsize);
				}
			}
			last_script = current_script;
		}

		if (strchr("., ", *p))
			last_character_was_word_separator = 1;
		else
			last_character_was_word_separator = 0;

		skip = utf8_charlen(p);
		if (skip > 1)
			p += skip - 1;
	}

	return points;
}

CMD_OVERRIDE_FUNC(override_msg)
{
	int score, ret;
	const char *text;
	char stripped[4096];

	if (!MyUser(client) || (parc < 3) || BadPtr(parv[2]) ||
	    user_allowed_by_security_group(client, cfg.except))
	{
		/* Short circuit for: remote clients, insufficient parameters,
		 * antimixedutf8::except.
		 */
		CALL_NEXT_COMMAND_OVERRIDE();
		return;
	}

	text = StripControlCodesEx(parv[2], stripped, sizeof(stripped), 0);
	score = lookalikespam_score(text, NULL, 0);
	if ((score >= cfg.score) && !find_tkl_exception(TKL_ANTIMIXEDUTF8, client))
	{
		char logbuf[512];
		int retval;

		/* Re-run with a log buffer and log... */
		logbuf[0] = '\0';
		lookalikespam_score(text, logbuf, sizeof(logbuf));
		unreal_log(ULOG_INFO, "antimixedutf8", "ANTIMIXEDUTF8_HIT", client,
		           "[antimixedutf8] Client $client.details hit score $score. Mixed scripts detected: $scripts",
		           log_data_integer("score", score),
		           log_data_string("scripts", logbuf));

		/* Take the action */
		retval = take_action(client, cfg.ban_action, cfg.ban_reason, cfg.ban_time, 0, NULL);
		if ((retval == BAN_ACT_WARN) || (retval == BAN_ACT_SOFT_WARN))
		{
			/* no action */
		} else
		if ((retval == BAN_ACT_BLOCK) || (retval == BAN_ACT_SOFT_BLOCK))
		{
			sendnotice(client, "%s", cfg.ban_reason);
			return;
		} else if (retval > 0)
		{
			return;
		}
		/* fallthrough for retval <=0 */
	}

	CALL_NEXT_COMMAND_OVERRIDE();
}

/*** rest is module and config stuff ****/

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, antimixedutf8_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	init_config();
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, antimixedutf8_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (!CommandOverrideAdd(modinfo->handle, "PRIVMSG", 0, override_msg))
		return MOD_FAILED;

	if (!CommandOverrideAdd(modinfo->handle, "NOTICE", 0, override_msg))
		return MOD_FAILED;

	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	free_config();
	return MOD_SUCCESS;
}

static void init_config(void)
{
	memset(&cfg, 0, sizeof(cfg));
	/* Default values */
	cfg.score = 10;
	safe_strdup(cfg.ban_reason, "Possible mixed character spam");
	cfg.ban_action = banact_value_to_struct(BAN_ACT_BLOCK);
	cfg.ban_time = 60 * 60 * 4; /* irrelevant for block, but some default for others */
}

static void free_config(void)
{
	safe_free(cfg.ban_reason);
	free_security_group(cfg.except);
	safe_free_all_ban_actions(cfg.ban_action);
	memset(&cfg, 0, sizeof(cfg)); /* needed! */
}

int antimixedutf8_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::antimixedutf8... */
	if (!ce || !ce->name || strcmp(ce->name, "antimixedutf8"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value)
		{
			config_error("%s:%i: set::antimixedutf8::%s with no value",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		} else
		if (!strcmp(cep->name, "score"))
		{
			int v = atoi(cep->value);
			if ((v < 1) || (v > 99))
			{
				config_error("%s:%i: set::antimixedutf8::score: must be between 1 - 99 (got: %d)",
					cep->file->filename, cep->line_number, v);
				errors++;
			}
		} else
		if (!strcmp(cep->name, "ban-action"))
		{
			errors += test_ban_action_config(cep);
		} else
		if (!strcmp(cep->name, "ban-reason"))
		{
		} else
		if (!strcmp(cep->name, "ban-time"))
		{
		} else
		if (!strcmp(cep->name, "except"))
		{
			test_match_block(cf, cep, &errors);
		} else
		{
			config_error("%s:%i: unknown directive set::antimixedutf8::%s",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		}
	}
	*errs = errors;
	return errors ? -1 : 1;
}

int antimixedutf8_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::antimixedutf8... */
	if (!ce || !ce->name || strcmp(ce->name, "antimixedutf8"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "score"))
		{
			cfg.score = atoi(cep->value);
		} else
		if (!strcmp(cep->name, "ban-action"))
		{
			if (cfg.ban_action)
				safe_free_all_ban_actions(cfg.ban_action);
			cfg.ban_action = parse_ban_action_config(cep);
		} else
		if (!strcmp(cep->name, "ban-reason"))
		{
			safe_strdup(cfg.ban_reason, cep->value);
		} else
		if (!strcmp(cep->name, "ban-time"))
		{
			cfg.ban_time = config_checkval(cep->value, CFG_TIME);
		} else
		if (!strcmp(cep->name, "except"))
		{
			conf_match_block(cf, cep, &cfg.except);
		}
	}
	return 1;
}
