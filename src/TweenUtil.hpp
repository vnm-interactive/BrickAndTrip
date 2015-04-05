﻿#pragma once

//
// Tweenセットアップ
//


namespace ngs {

template <typename T>
void setTweenOption(typename ci::Tween<T>::Options& option, const ci::JsonTree& param) {
  if (param.hasChild("loop")) {
    option.loop(param["loop"].getValue<bool>());
  }
  if (param.hasChild("ping-pong")) {
    option.pingPong(param["ping-pong"].getValue<bool>());
  }
  if (param.hasChild("delay")) {
    option.delay(param["delay"].getValue<float>());
  }
}


void setVec3Tween(ci::Timeline& timeline,
                  ci::Anim<ci::Vec3f>& value, const ci::JsonTree& param, const bool is_first) {
  auto start = param.hasChild("start") ? Json::getVec3<float>(param["start"])
                                       : value();
  auto end = param.hasChild("end") ? Json::getVec3<float>(param["end"])
                                   : value();

  auto option = is_first ? timeline.apply(&value,
                                          start, end,
                                          param["duration"].getValue<float>(),
                                          getEaseFunc(param["type"].getValue<std::string>()))
                         : timeline.appendTo(&value,
                                             end,
                                             param["duration"].getValue<float>(),
                                             getEaseFunc(param["type"].getValue<std::string>()));

  if (is_first) value = start;
  setTweenOption<ci::Vec3f>(option, param);
}
  
void setColorTween(ci::Timeline& timeline,
                   ci::Anim<ci::Color>& value, const ci::JsonTree& param, const bool is_first) {
  auto start = param.hasChild("start") ? Json::getColor<float>(param["start"])
                                       : value();
  auto end = param.hasChild("end") ? Json::getColor<float>(param["end"])
                                   : value();

  auto option = is_first ? timeline.apply(&value,
                                          start, end,
                                          param["duration"].getValue<float>(),
                                          getEaseFunc(param["type"].getValue<std::string>()))
                         : timeline.appendTo(&value,
                                             end,
                                             param["duration"].getValue<float>(),
                                             getEaseFunc(param["type"].getValue<std::string>()));

  if (is_first) value = start;
  setTweenOption<ci::Color>(option, param);
}


void setVec3Tween(ci::Timeline& timeline,
                  ci::Anim<ci::Vec3f>& value, const ci::JsonTree& param,
                  const float cube_size, const bool is_first) {
  auto start = param.hasChild("start") ? Json::getVec3<float>(param["start"]) * cube_size + value()
                                       : value();
  auto end = param.hasChild("end") ? Json::getVec3<float>(param["end"]) * cube_size + value()
                                   : value();

  auto option = is_first ? timeline.apply(&value,
                                          start, end,
                                          param["duration"].getValue<float>(),
                                          getEaseFunc(param["type"].getValue<std::string>()))
                         : timeline.appendTo(&value,
                                             end,
                                             param["duration"].getValue<float>(),
                                             getEaseFunc(param["type"].getValue<std::string>()));

  if (is_first) value = start;
  setTweenOption<ci::Vec3f>(option, param);
}

}