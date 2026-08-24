#include "numberedit.h"

using namespace Tempest;

NumberEdit::NumberEdit() {
  setText("0");
  }

void NumberEdit::keyDownEvent(Tempest::KeyEvent& e) {
  if(e.key==Event::K_Return) {
    onEnter();
    return;
    }

  if(!checkInput(e))
    return;
  LineEdit::keyDownEvent(e);
  postAccept(e);
  }

void NumberEdit::keyRepeatEvent(KeyEvent& e) {
  if(!checkInput(e))
    return;
  LineEdit::keyRepeatEvent(e);
  postAccept(e);
  }

bool NumberEdit::checkInput(KeyEvent& e) const {
  if(e.key==Event::K_Delete || e.key==Event::K_Back)
    return true;
  return ('0'<=e.code && e.code<='9') || e.code=='-' || e.code==0 || (e.code=='.' && flt);
  }

void NumberEdit::focusEvent(FocusEvent& e) {
  if(!e.in)
    commitChange();
  }

void NumberEdit::commitChange() {
  onValueModifyed(val,true);
  }

void NumberEdit::postAccept(KeyEvent&) {
  const char*  t   = text().c_str();
  char*        end = nullptr;
  size_t       len = std::strlen(t);

  const float ret = std::strtof(t,&end);
  if(end!=t+len || len==0) {
    if(std::strcmp(t,"-")!=0) {
      setValue(val);
      return;
      }
    }
  if(val==ret)
    return;
  val = ret;
  onValueModifyed(ret,false);
  }

void NumberEdit::setValue(double v) {
  char buf[64]={};
  if(flt) {
    char fmt[64] = {};
    std::snprintf(fmt,sizeof(fmt),"%%0.%dg",digits);
    val = float(v);
    std::snprintf(buf,sizeof(buf),fmt,v);
    } else {
    val = int(v);
    std::snprintf(buf,sizeof(buf),"%d",int(v));
    }
  setText(buf);
  }

double NumberEdit::value() const {
  return val;
  }

void NumberEdit::enableFloats(bool e) {
  flt = e;
  if(!e)
    setValue(int(val));
  }

void NumberEdit::setFloatDigits(uint8_t d) {
  digits = d;
  setValue(val);
  }
