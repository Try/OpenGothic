# Doc_PrintLine vs Doc_PrintLines: missing single-line/word-wrap distinction

**Confidence:** Medium (divergence is certain and well-evidenced; fix is structural, so status is DEFERRED rather than a surgical patch).

## Original function + address (prose)

In `Gothic2.exe` the two book/document externals route to distinct line builders:

- `oCDocumentManager::PrintLine` @0x0065f280 walks to the target page-view and calls
  `oCViewDocument::PrintLine` @0x0068cbb0, which ultimately calls
  `zCViewPrint::PrintLine` @0x00693a50.
- `oCDocumentManager::PrintLines` @0x0065f2c0 calls `oCViewDocument::PrintLines` @0x0068cc70,
  which calls `zCViewPrint::PrintLines` @0x00693c00.

`zCViewPrint::PrintLine` @0x00693a50 creates exactly **one** `zCViewText2` line object for the
whole string, then advances the print cursor by one font-Y. There is no width measurement: a string
wider than the page is emitted as a single line and simply overflows / is clipped at the page edge.

`zCViewPrint::PrintLines` @0x00693c00 is a **word-wrapping** builder. It computes the page's right
boundary from the view rect (and subtracts the page margins), then tokenizes the string word by
word (`zCFont::GetFontX` per word). Each time the accumulated width would exceed the right boundary
it flushes the current line as a `zCViewText2`, advances the cursor by one font-Y, and starts a new
line — so one `PrintLines` call can produce several wrapped display lines that all fit the page
width.

So the engine contract is: **`Doc_PrintLine` = exactly one display line, no wrapping (overflow);
`Doc_PrintLines` = automatic word-wrap to the current page width.** The wrap is computed at draw
time from the live page geometry, not from anything the script supplies.

Both view-level builders also share the page-index convention seen across the Doc_* setters
(`page < 0` ⇒ apply to every page; `page >= 0` ⇒ the page-th child only).

## OpenGothic file:line

- `game/gothic.cpp:1133` `Gothic::doc_printline`
- `game/gothic.cpp:1141` `Gothic::doc_printlines`
- `game/ui/documentmenu.cpp:94` `GthFont::drawText(... i.text ...)` (the render path)
- `game/utils/gthfont.cpp:191` `GthFont::getLine` (the word-wrap used by that render path)

## Divergence

`doc_printline` and `doc_printlines` are byte-for-byte identical in OpenGothic — each just does
`pages[page].text += text; pages[page].text += "\n";`. They both append plain text plus a single
newline into one flat per-page `std::string`.

At paint time `DocumentMenu::paintEvent` draws that whole blob with
`fnt->drawText(p, x+mgr.left, ..., w - mgr.xMargin(), h - mgr.yMargin(), i.text, AlignLeft)`, and
`GthFont::getLine` (game/utils/gthfont.cpp:191) **word-wraps at the box width `bw` for every
line**. Net effect:

- `Doc_PrintLines` text wraps — correct, matches `zCViewPrint::PrintLines`.
- `Doc_PrintLine` text **also wraps** — wrong. The original emits one non-wrapping line that
  overflows the page; OpenGothic silently re-flows a too-wide `PrintLine` string onto multiple
  lines, shifting all following lines down and changing the page layout.

The two externals therefore have a real behavioral difference in the original (one wraps, one does
not) that OpenGothic collapses into a single wrapping behavior. The visible delta only appears when
a `Doc_PrintLine` argument is wider than the rendered page; for content that already fits, the two
paths coincide.

## Proposed patch

**DEFERRED.**

Reason: the wrap decision is made at *render* time from the live page width (background-texture
size × interface scale − margins), which is not known at script-call time inside `doc_printline` /
`doc_printlines`. To honor the original contract OpenGothic would have to remember, per inserted
chunk, whether it came from `PrintLine` (never wrap) or `PrintLines` (wrap), and have the renderer
apply or suppress wrapping per chunk. `DocumentMenu::Page` (game/ui/documentmenu.h:20) currently
stores only a single flat `std::string text` with no per-line/per-chunk metadata, and
`GthFont::drawText` wraps unconditionally on `bw`. A correct fix requires:

1. a per-line/per-chunk "no-wrap" marker on `DocumentMenu::Page` (e.g. a parallel list of
   {string, bool wrap} segments instead of one concatenated string), and
2. a `GthFont::drawText` path (or `getLine` flag) that emits a single un-wrapped line for the
   no-wrap segments while keeping current behavior for the wrap segments.

That is a data-structure + renderer change spanning `documentmenu.h`, `documentmenu.cpp`,
`gothic.cpp`, and `gthfont.cpp` — beyond a surgical, locally build-verifiable edit — so it is
deferred rather than patched here.

<!--
NOTE: in original-game zCViewPrint::PrintLine @0x00693a50 emits a single non-wrapping zCViewText2
per call (no width check), whereas zCViewPrint::PrintLines @0x00693c00 word-wraps to the page's
right boundary; OpenGothic doc_printline/doc_printlines (game/gothic.cpp:1133/1141) are identical
and the renderer (gthfont.cpp:191 getLine) word-wraps both.
-->
