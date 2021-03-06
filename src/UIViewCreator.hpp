﻿#pragma once

//
// UIView生成
//

#include <boost/noncopyable.hpp>
#include "UIView.hpp"
#include "JsonUtil.hpp"


namespace ngs {

class UIViewCreator : private boost::noncopyable {
  ci::TimelineRef timeline_;

  ci::JsonTree& params_;
  
  ci::Camera& camera_;

  Autolayout& autolayout_;
  Event<EventParam>& event_;
  Event<std::vector<Touch> >& touch_event_;


public:
  UIViewCreator(ci::JsonTree& params,
                ci::TimelineRef timeline,
                ci::Camera& camera,
                Autolayout& autolayout,
                Event<EventParam>& event,
                Event<std::vector<Touch> >& touch_event) noexcept :
    timeline_(timeline),
    params_(params),
    camera_(camera),
    autolayout_(autolayout),
    event_(event),
    touch_event_(touch_event)
  {}


  std::unique_ptr<UIView> create(const std::string& path) noexcept {
    // ちょくちょくAutolayoutのお掃除 
    autolayout_.eraseInvalid();

    // 難読化のためにparam.jsonと同じ実装を利用
    auto param = Params::load(path);
      
    return std::unique_ptr<UIView>(new UIView(params_,
                                              param,
                                              timeline_,
                                              camera_, autolayout_, event_, touch_event_));
  }


private:

  
};

}
