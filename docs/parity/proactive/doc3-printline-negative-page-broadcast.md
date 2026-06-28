# Doc_PrintLine / Doc_PrintLines: a negative page index broadcasts to every page in the original, OpenGothic drops the line

**Confidence:** High that the divergence exists (verified directly in the original
view-level builders and confirmed as a system-wide convention across five Doc_*
methods). Medium overall, because observability in stock content depends on whether a
script passes `page < 0`. The proposed fix is strictly **additive** — it only adds
behavior for the currently-dropped `page < 0` case and does **not** touch the
single-line-vs-wrap rendering path, so it cannot collide with the already-deferred
`doc2-printline-vs-printlines-wrap` item.

## Original function + address (prose only)

`oCViewDocument::PrintLine` at **0x0068cbb0** and `oCViewDocument::PrintLines` at
**0x0068cc70** (reached from `oCDocumentManager::PrintLine` @0x0065f280 /
`oCDocumentManager::PrintLines` @0x0065f2c0, which first walk the document list to the
requested document handle, then forward the page index and text to the view).

What the view-level builders do with the page index:

1. If the page index is **negative** (`< 0`), the routine counts the document's page
   children and then iterates over **all** of them, calling `zCViewPrint::PrintLine`
   (resp. `zCViewPrint::PrintLines`) on each page — i.e. the line is appended to
   *every* page of the document.
2. If the page index is **non-negative**, it resolves a single page child via
   `zCViewObject::GetChild(page)` and appends the line to that one page only; if no
   such child exists it is a no-op.

This "`page < 0` ⇒ apply to every page; `page >= 0` ⇒ the page-th child only"
convention is not unique to text: the same negative-index broadcast is implemented
identically in `oCViewDocument::SetFont` @0x0068caf0, `oCViewDocument::SetMargins`
@0x0068c7f0, and `oCViewDocument::SetPage` @0x0068c970. It is a deliberate, system-wide
contract of the document subsystem. (For a single-page document, the broadcast and the
explicit `page == 0` call therefore produce the same result — printing to the one page.)

## OpenGothic file:line

- `game/gothic.cpp:1133` `Gothic::doc_printline`
- `game/gothic.cpp:1141` `Gothic::doc_printlines`

```
game/gothic.cpp:1135   if(doc!=nullptr && page>=0 && size_t(page)<doc->pages.size()){
game/gothic.cpp:1143   if(doc!=nullptr && page>=0 && size_t(page)<doc->pages.size()){
```

## Divergence

Both OpenGothic externals guard on `page >= 0`. A call with a **negative** page index
fails that guard and is silently discarded — the text is never stored on any page, so
it never renders. The original instead appends the line to **every** page of the
document. The two diverge whenever a script uses `Doc_PrintLine(doc, -1, text)` /
`Doc_PrintLines(doc, -1, text)`: original shows the text (on all pages — in the common
single-page case, on that page); OpenGothic shows nothing, yielding a blank document.

This is a different defect from the deferred `doc2-printline-vs-printlines-wrap` item:
that item concerns whether a single `Doc_PrintLine` string word-wraps at draw time
(it should not, but OpenGothic wraps it). The present item concerns the *page
selection* — the data being dropped entirely before it ever reaches the renderer. The
deferred wrap analysis only notes the broadcast convention in passing; it neither
diagnoses nor fixes the negative-page data loss.

## Proposed patch (additive; does not alter wrap behavior)

`game/gothic.cpp`

OLD:
```
void Gothic::doc_printline(int handle, int page, std::string_view text) {
  auto& doc = getDocument(handle);
  if(doc!=nullptr && page>=0 && size_t(page)<doc->pages.size()){
    doc->pages[size_t(page)].text += text;
    doc->pages[size_t(page)].text += "\n";
    }
  }

void Gothic::doc_printlines(int handle, int page, std::string_view text) {
  auto& doc = getDocument(handle);
  if(doc!=nullptr && page>=0 && size_t(page)<doc->pages.size()){
    doc->pages[size_t(page)].text += text;
    doc->pages[size_t(page)].text += "\n";
    }
  }
```

NEW:
```
void Gothic::doc_printline(int handle, int page, std::string_view text) {
  // NOTE: in original-game oCViewDocument::PrintLine @0x68cbb0 (via
  // oCDocumentManager::PrintLine @0x65f280) a negative page index broadcasts the line
  // to *every* page of the document; a non-negative index targets that single page.
  // OpenGothic silently dropped negative-page calls.
  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;
  if(page<0){
    for(auto& pg:doc->pages){
      pg.text += text;
      pg.text += "\n";
      }
    return;
    }
  if(size_t(page)<doc->pages.size()){
    doc->pages[size_t(page)].text += text;
    doc->pages[size_t(page)].text += "\n";
    }
  }

void Gothic::doc_printlines(int handle, int page, std::string_view text) {
  // NOTE: in original-game oCViewDocument::PrintLines @0x68cc70 (via
  // oCDocumentManager::PrintLines @0x65f2c0) a negative page index broadcasts the line
  // to *every* page; a non-negative index targets that single page. OpenGothic
  // silently dropped negative-page calls.
  auto& doc = getDocument(handle);
  if(doc==nullptr)
    return;
  if(page<0){
    for(auto& pg:doc->pages){
      pg.text += text;
      pg.text += "\n";
      }
    return;
    }
  if(size_t(page)<doc->pages.size()){
    doc->pages[size_t(page)].text += text;
    doc->pages[size_t(page)].text += "\n";
    }
  }
```
