﻿#pragma once

//
// Settings画面
//

#include "ControllerBase.hpp"
#include "UIView.hpp"
#include "ConnectionHolder.hpp"


namespace ngs {

class SettingsController : public ControllerBase {
  ci::JsonTree& params_;
  Event<EventParam>& event_;
  Records& records_;

  std::unique_ptr<UIView> view_;
  
  bool active_;

  ConnectionHolder connections_;

  ci::TimelineRef event_timeline_;

  
public:
  SettingsController(ci::JsonTree& params,
                    ci::TimelineRef timeline,
                    Event<EventParam>& event,
                    Records& records,
                     std::unique_ptr<UIView>&& view) :
    params_(params),
    event_(event),
    records_(records),
    view_(std::move(view)),
    active_(true),
    event_timeline_(ci::Timeline::create())
  {
    DOUT << "SettingsController()" << std::endl;

    auto current_time = timeline->getCurrentTime();
    event_timeline_->setStartTime(current_time);
    timeline->apply(event_timeline_);

    connections_ += event.connect("se-change",
                                  [this](const Connection& connection, EventParam& param) {
                                    setSoundIcon("se-setting", records_.toggleSeOn());
                                  });

    connections_ += event.connect("bgm-change",
                                  [this](const Connection& connection, EventParam& param) {
                                    setSoundIcon("bgm-setting", records_.toggleBgmOn());
                                  });

    connections_ += event.connect("settings-agree",
                                  [this](const Connection& connection, EventParam& param) {
                                    records_.write(params_["game.records"].getValue<std::string>());

                                    // 時間差tween & Title起動
                                    event_timeline_->add([this]() {
                                        view_->startWidgetTween("tween-out");
                                        event_.signal("begin-title", EventParam());
                                      },
                                      event_timeline_->getCurrentTime() + 0.5f);
                                    
                                    // 時間差でControllerを破棄
                                    event_timeline_->add([this]() {
                                        
                                        active_ = false;
                                      },
                                      event_timeline_->getCurrentTime() + 1.0f);
                                  });
    
    setSoundIcon("se-setting", records_.isSeOn());
    setSoundIcon("bgm-setting", records_.isBgmOn());

    view_->startWidgetTween("tween-in");
  }

  ~SettingsController() {
    DOUT << "~SettingsController()" << std::endl;

    // 再生途中のものもあるので、手動で取り除く
    event_timeline_->removeSelf();
  }


private:
  bool isActive() const override { return active_; }

  Event<EventParam>& event() override { return event_; }

  void resize() override {
  }
  
  void update(const double progressing_seconds) override {
  }
  
  void draw(FontHolder& fonts) override {
    view_->draw(fonts);
  }


  void setSoundIcon(const std::string& widget, const bool is_sound) {
    view_->getWidget(widget).getCubeText().setText(is_sound ? "z"
                                                            : "x");
  }
  
};

}
