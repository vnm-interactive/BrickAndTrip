﻿#pragma once

//
// boost::signals2を利用した汎用的なイベント
//

#include <boost/signals2.hpp>
#include <map>


namespace ngs {

using Connection = boost::signals2::connection;

template <typename... Args>
class Event {
  using SignalType = boost::signals2::signal<void(Args&...)>;

  std::map<std::string, SignalType> signals_;


public:
  Event() = default;


  template<typename F>
  Connection connect(const std::string& msg, F callback) {
    return signals_[msg].connect(callback);
  }  

  
  template <typename... Args2>
  void signal(const std::string& msg, Args2&&... args) {
    signals_[msg](std::forward<Args2>(args)...);
  }

  
private:
  // TIPS:コピー不可
  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;
  
};

}
