#include "questionbox.h"

#include <Tempest/Button>
#include <Tempest/Label>
#include <Tempest/Painter>
#include <Tempest/Application>

#include "ui/uihelper.h"
#include "assets.h"

using namespace Tempest;

struct QuestionBox::Title : Widget {
  void paintEvent(PaintEvent& e) override {
    Painter p(e);
    p.setFont(fnt);

    auto sz = fnt.textSize(title);
    p.drawText((w()-sz.w)/2,(h()+sz.h)/2,title);
    }

  void setText(const char* t) {
    title = t;
    update();
    }

  void setFont(const Tempest::Font& f) {
    fnt = f;
    update();
    }

  Tempest::Font fnt;
  std::string   title;
  };

QuestionBox::QuestionBox() {
  resize(500,200);

  auto fnt = Assets::inst().fntApp;
  //fnt.setPixelSize(24);
  fnt.setBold(true);

  title = &addWidget(new Title());
  title->setSizePolicy(Preferred,Fixed);
  title->setMinimumSize(0,128);
  title->setFont(fnt);
  fnt.setBold(false);

  auto& bot = addWidget(new Widget());
  bot.setSizePolicy(Preferred,Fixed);
  bot.setLayout(Horizontal);

  yes    = &bot.addWidget(new Button());
  no     = &bot.addWidget(new Button());
  ok     = &bot.addWidget(new Button());
  cancel = &bot.addWidget(new Button());

  setLayout(Vertical);

  yes   ->setText("Yes");
  no    ->setText("No");
  ok    ->setText("OK");
  cancel->setText("Cancel");

  yes   ->setButtonType(Button::T_FlatButton);
  no    ->setButtonType(Button::T_FlatButton);
  ok    ->setButtonType(Button::T_FlatButton);
  cancel->setButtonType(Button::T_FlatButton);

  yes   ->onClick.bind(this,&QuestionBox::onButton<Yes>);
  no    ->onClick.bind(this,&QuestionBox::onButton<No>);
  ok    ->onClick.bind(this,&QuestionBox::onButton<OK>);
  cancel->onClick.bind(this,&QuestionBox::onButton<Cancel>);

  bot.setMinimumSize(yes->sizeHint());
  auto wr = UiHelper::wrapContent(*this,Vertical);
  resize(std::max(wr.w,w()), wr.h+margins().yMargin());
  }

QuestionBox::Ret QuestionBox::ask(const std::string& title, QuestionBox::Ret buttons) {
  return ask(title.c_str(),buttons);
  }

template<QuestionBox::Ret r>
void QuestionBox::onButton() {
  ret = r;
  close();
  }

QuestionBox::Ret QuestionBox::ask(const char* title, Ret buttons) {
  QuestionBox box;
  box.setTitle(title);

  box.yes   ->setVisible(buttons & Yes);
  box.no    ->setVisible(buttons & No);
  box.ok    ->setVisible(buttons & OK);
  box.cancel->setVisible(buttons & Cancel);

  box.exec();
  return box.ret;
  }

void QuestionBox::setTitle(const char* t) {
  title->setText(t);
  auto wr = UiHelper::wrapContent(*this,Vertical);
  resize(std::max(wr.w,w()), wr.h+margins().yMargin());
  }

