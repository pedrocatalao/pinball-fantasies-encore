# Question: how many numbers did the engine's value-naming replace, by kind? Counts, in the
# added lines of `git diff -U0 -- src` (diff.txt) that are not definitions or comments, each
# use of a new name, and how many numeric literals the removed lines had against the added.
#   usage: git diff -U0 <before> -- src > diff.txt && python3 tools/probes/pfr-names/count_literals.py diff.txt
import re, sys
lines = open(sys.argv[1]).read().splitlines()
plus = [l[1:] for l in lines if l.startswith('+') and not l.startswith('+++')]
minus = [l[1:] for l in lines if l.startswith('-') and not l.startswith('---')]
def code(l):
    s = l.strip()
    return not (s.startswith('constexpr') or s.startswith('///') or s.startswith('namespace') or s.startswith('}  //') or s.startswith('static constexpr'))
plus_code = [re.sub(r'//.*', '', l) for l in plus if code(l)]
minus_code = [re.sub(r'//.*', '', l) for l in minus]
kinds = {
  'flags (kSet)': [r'\bkSet\b'],
  'none / end of a list (kNone)': [r'\bkNone\b'],
  'times (frames::, idle, tilt)': [r'frames::', r'tilt::'],
  'speeds, positions, the playfield': [r'playfield::', r'plunger::', r'nudge::', r'screenScroll::', r'bounces::', r'ballAngles::', r'slopeRec::'],
  'the display (places, sizes, fonts)': [r'display::', r'font::', r'animRec::'],
  'characters (glyph::, chars)': [r'glyph::', r"'Y'", r"'0'", r'\bmatchSeen\b', r'kWordEnd'],
  'counts and sizes': [r'kScoreDigits', r'kDecimal\b', r'bestScoreRec::', r'kTimerCounters', r'flipperRec::(third|count|quant)', r'material::(count|size|gains|minBounceSpeed)', r'ballsPerGame::', r'kCheatLetters', r'kMostPlayers', r'kPointBytes', r'kProbeCount', r'zoneRec::boxOnly', r'kSineWords', r'mz::', r'video::(rowBytes|rowShown|rowSpare|paletteBytes|colourBytes|planes)\b', r'kTables\b'],
  'keys (scancodes::, keyBits::, scrollKey::)': [r'scancodes::', r'keyBits::', r'scrollKey::'],
  'materials, masks, layers, bits': [r'material::(markBit|wallBit|slopeBit|flipper|kicker|bumper)', r'maskCopy::', r'layers::', r'flipperRec::(right|left)\b', r'kNibble', r'kAcrossRight', r'kFirstBit', r'planeMask::'],
  'colours, video card': [r'video::(colourMax|splitLine)', r'kLightScale', r'driverStatus::'],
  'opcodes': [r'x86::'],
  'other sentinels and choices': [r'taskState::', r'tableAngle::', r'tableId::', r'scrollOption::', r'kRowEnd', r'kCheatsEnd', r'kNoPicture', r'noSlope', r'kRepeats', r'kPolled', r'kDebugTrace', r'kUnused', r'kChunkHead', r'kStripSearched', r'kDecimalAdjust', r'flippersRow', r'thirdFlipperRow'],
}
total = 0
for k, pats in kinds.items():
    n = sum(len(re.findall(p, l)) for l in plus_code for p in pats)
    total += n
    print(f'{k:45} {n}')
print(f'{"total uses":45} {total}')
lit = re.compile(r'(?<![\w.])(0x[0-9a-fA-F]+|\d+)(?![\w.])')
print('numeric literals in removed lines:', sum(len(lit.findall(l)) for l in minus_code))
print('numeric literals in added code lines:', sum(len(lit.findall(l)) for l in plus_code))
