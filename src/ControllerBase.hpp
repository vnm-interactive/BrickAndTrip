﻿#pragma once

//
// 
//

#include "Event.hpp"
#include "EventParam.hpp"
#include "FontHolder.hpp"
#include "ModelHolder.hpp"
#include <boost/noncopyable.hpp>


namespace ngs {

struct ControllerBase : private boost::noncopyable {
  virtual ~ControllerBase() = default;

  virtual bool isActive() const = 0;

  virtual Event<EventParam>& event() = 0;

  virtual void resize() = 0;
  
  virtual void update(const double progressing_seconds) = 0;
  virtual void draw(FontHolder& fonts, ModelHolder& models) = 0;
};

}
