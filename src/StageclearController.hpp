﻿#pragma once

//
// Stage clear
//

#include "ControllerBase.hpp"
#include "UIView.hpp"
#include "ConnectionHolder.hpp"
#include "Share.h"
#include "Capture.h"


namespace ngs {

class StageclearController : public ControllerBase {
  ci::JsonTree& params_;
  Event<EventParam>& event_;

  float tween_delay_;
  float event_delay_;
  float deactive_delay_;
  float sns_delay_;

  std::unique_ptr<UIView> view_;
  
  bool active_;
  bool all_cleard_;
  bool regular_stage_;
  bool all_stage_;

  // FIXME:全ステージクリア時の情報を保持している
  int total_score_;
  bool highest_score_;
  int item_num_;
  int item_total_num_;
  bool highest_item_num_;

  std::string sns_text_;

  ci::Anim<double> clear_time_;
  ci::Anim<int> item_rate_;
  ci::Anim<int> score_;
  
  ConnectionHolder connections_;

  ci::TimelineRef event_timeline_;


public:
  StageclearController(ci::JsonTree& params,
                       ci::TimelineRef timeline,
                       Event<EventParam>& event,
                       const EventParam& result,
                       std::unique_ptr<UIView>&& view) :
    params_(params),
    event_(event),
    tween_delay_(params["stageclear.tween_delay"].getValue<float>()),
    event_delay_(params["stageclear.event_delay"].getValue<float>()),
    deactive_delay_(params["stageclear.deactive_delay"].getValue<float>()),
    sns_delay_(params["stageclear.sns_delay"].getValue<float>()),
    view_(std::move(view)),
    active_(true),
    all_cleard_(boost::any_cast<bool>(result.at("all_cleared"))),
    regular_stage_(boost::any_cast<bool>(result.at("regular_stage"))),
    all_stage_(boost::any_cast<bool>(result.at("all_stage"))),
    total_score_(boost::any_cast<int>(result.at("total_score"))),
    highest_score_(boost::any_cast<bool>(result.at("highest_total_score"))),
    item_num_(boost::any_cast<int>(result.at("play_item_num"))),
    item_total_num_(boost::any_cast<int>(result.at("play_item_total_num"))),
    highest_item_num_(boost::any_cast<bool>(result.at("highest_item_num"))),
    sns_text_(params["stageclear.sns_text"].getValue<std::string>()),
    clear_time_(0.0),
    item_rate_(0),
    score_(0),
    event_timeline_(ci::Timeline::create())
  {
    DOUT << "StageclearController()" << std::endl;
    
    auto current_time = timeline->getCurrentTime();
    event_timeline_->setStartTime(current_time);
    timeline->apply(event_timeline_);

    connections_ += event.connect("selected-agree",
                                  [this](const Connection& connection, EventParam& param) {
                                    view_->setActive(false);
                                    
                                    event_timeline_->add([this]() {
                                        view_->startWidgetTween("tween-out");

                                        event_timeline_->add([this]() {
                                            // クリア状況でmessageが違う
                                            std::string msg = "stageclear-agree";
                                            if (all_cleard_) {
                                              if (regular_stage_)  msg = "begin-regulat-stageclear";
                                              else if (all_stage_) msg = "begin-all-stageclear";
                                            }

                                            EventParam params = {
                                              { "total_score",       total_score_ },
                                              { "highest_score",     highest_score_ },
                                              { "item_num",          item_num_ },
                                              { "item_total_num",    item_total_num_ },
                                              { "highest_item_num",  highest_item_num_ },
                                            };
                                            
                                            event_.signal(msg, params);

                                            event_timeline_->add([this]() {
                                                active_ = false;
                                              },
                                              event_timeline_->getCurrentTime() + deactive_delay_);
                                          },
                                          event_timeline_->getCurrentTime() + event_delay_);
                                      },
                                      event_timeline_->getCurrentTime() + tween_delay_);
                                  });

#if defined(CINDER_COCOA_TOUCH)
    connections_ += event.connect("selected-share",
                                  [this](const Connection& connection, EventParam& param) {
                                    view_->setActive(false);
                                    event_.signal("sns-post-begin", EventParam());
                                    
                                    event_timeline_->add([this]() {
                                        DOUT << "Share" << std::endl;
                                        
                                        Share::post(sns_text_,
                                                    captureTopView(),
                                                    [this]() {
                                                      event_.signal("sns-post-end", EventParam());
                                                      view_->setActive(true);
                                                    });
                                      },
                                      event_timeline_->getCurrentTime() + sns_delay_);
                                  });
#endif
    
    setupView(params, result);

    view_->startWidgetTween("tween-in");

    if (params.hasChild("stageclear.active_delay")) {
      view_->setActive(false);

      float delay = params["stageclear.active_delay"].getValue<float>();
      event_timeline_->add([this]() {
          view_->setActive(true);
        },
        event_timeline_->getCurrentTime() + delay);
    }
  }

  ~StageclearController() {
    DOUT << "~StageclearController()" << std::endl;

    // 再生途中のものもあるので、手動で取り除く
    event_timeline_->removeSelf();
  }


private:
  void setupView(ci::JsonTree& params,
                 const EventParam& result) {
    auto ease_func     = getEaseFunc(params_["stageclear.countup_ease_name"].getValue<std::string>());
    auto ease_duration = params_["stageclear.countup_ease_duration"].getValue<float>();

    {
      // constなのでatを使っている
      auto clear_time = boost::any_cast<double>(result.at("clear_time"));
      auto options = event_timeline_->apply(&clear_time_,
                                            0.0, clear_time,
                                            ease_duration, ease_func);

      options.delay(params_["stageclear.clear_time_delay"].getValue<float>());

      options.updateFn([this]() {
          view_->getWidget("time-result").setText(toFormatedString(clear_time_()));
        });
      // TODO:最速タイム時の演出
    }

    {
      auto item_num = boost::any_cast<int>(result.at("item_num"));
      auto item_total_num = boost::any_cast<int>(result.at("item_total_num"));

      if (item_total_num > 0) {
        int item_rate = item_num * 100 / item_total_num;

        auto options = event_timeline_->apply(&item_rate_,
                                              0, item_rate,
                                              ease_duration, ease_func);

        options.delay(params_["stageclear.item_rate_delay"].getValue<float>());

        options.updateFn([this]() {
            view_->getWidget("item-result").setText(toFormatedString(item_rate_(), 3) + "%", false);
          });

        if (boost::any_cast<bool>(result.at("complete_item"))) {
          options.finishFn([this]() {
              view_->startWidgetTween("tween-complete-item");
            });
        }
      }
    }

    {
      auto score = boost::any_cast<int>(result.at("score"));

      auto options = event_timeline_->apply(&score_,
                                            0, score,
                                            ease_duration, ease_func);

      options.delay(params_["stageclear.score_delay"].getValue<float>());

      options.updateFn([this]() {
          view_->getWidget("score-result").setText(toFormatedString(score_(), 4), false);
        });

      if (boost::any_cast<bool>(result.at("highest_score"))) {
          options.finishFn([this]() {
              view_->startWidgetTween("tween-highest-score");
            });
      }
    }

    {
      auto& rank_text = params["stageclear.rank"];
      auto rank = boost::any_cast<int>(result.at("rank"));
      
      view_->getWidget("rank-result").setText(rank_text[rank].getValue<std::string>());

      if (boost::any_cast<bool>(result.at("highest_rank"))) {
        view_->startWidgetTween("tween-highest-rank");
      }
    }
    
#if defined(CINDER_COCOA_TOUCH)
    if (canCaptureTopView()) {
      if (Share::canPost()) {
        auto& widget = view_->getWidget("share");
        
        widget.setDisp(true);
        widget.setActive(true);
      }
    }
#endif
  }

  
  bool isActive() const override {
    return active_;
  }

  Event<EventParam>& event() override { return event_; }

  void resize() override {
  }
  
  void update(const double progressing_seconds) override {
  }
  
  void draw(FontHolder& fonts, ModelHolder& models) override {
    view_->draw(fonts, models);
  }

};

}
