﻿#pragma once

//
// 操作可能なCube
//

#include "cinder/Timeline.h"
#include "cinder/Rand.h"
#include "EasingUtil.hpp"
#include "Utility.hpp"


namespace ngs {

class PickableCube {
public:
  enum {
    MOVE_NONE = -1,

    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT
  };

  
private:
  Event<EventParam>& event_;
  
  bool active_;

  u_int id_;
  
  float cube_size_;
  ci::Color color_;

  ci::Vec3i block_position_;
  
  ci::Anim<ci::Vec3f> position_;
  ci::Anim<ci::Quatf> rotation_;

  ci::TimelineRef animation_timeline_;

  bool can_pick_;
  bool picking_;
  bool on_stage_;

  int       move_direction_;
  ci::Vec3i move_vector_;
  int       move_speed_;
  
  std::string rotate_ease_;
  float rotate_duration_;
  std::vector<float> rotate_speed_rate_;
  ci::Anim<ci::Quatf> move_rotation_;

  std::string fall_ease_;
  float fall_duration_;
  float fall_y_;
  

public:
  PickableCube(const ci::JsonTree& params, Event<EventParam>& event,
               const ci::Vec3i& entry_pos) :
    event_(event),
    active_(true),
    id_(getUniqueNumber()),
    cube_size_(params["game.cube_size"].getValue<float>()),
    color_(Json::getColor<float>(params["game.pickable.color"])),
    block_position_(entry_pos),
    rotation_(ci::Quatf::identity()),
    animation_timeline_(ci::Timeline::create()),
    can_pick_(false),
    picking_(false),
    on_stage_(false),
    move_direction_(MOVE_NONE),
    move_vector_(ci::Vec3i::zero()),
    move_speed_(0),
    rotate_ease_(params["game.pickable.rotate_ease"].getValue<std::string>()),
    rotate_duration_(params["game.pickable.rotate_duration"].getValue<float>()),
    fall_ease_(params["game.pickable.fall_ease"].getValue<std::string>()),
    fall_duration_(params["game.pickable.fall_duration"].getValue<float>()),
    fall_y_(params["game.pickable.fall_y"].getValue<float>())
  {
    position_ = ci::Vec3f(block_position_) * cube_size_;
    // block_positionが同じ高さなら、StageCubeの上に乗るように位置を調整
    position_().y += cube_size_;

    for (const auto& param : params["game.pickable.rotate_speed_rate"]) {
      rotate_speed_rate_.push_back(param.getValue<float>());
    }
    
    auto current_time = ci::app::timeline().getCurrentTime();
    animation_timeline_->setStartTime(current_time);
    ci::app::timeline().apply(animation_timeline_);

    // 登場演出
    auto entry_y = Json::getVec2<float>(params["game.pickable.entry_y"]);
    float y = ci::randFloat(entry_y.x, entry_y.y) * cube_size_;
    ci::Vec3f start_value(position() + ci::Vec3f(0, y, 0));
    auto options = animation_timeline_->apply(&position_,
                                              start_value, position_(),
                                              params["game.pickable.entry_duration"].getValue<float>(),
                                              getEaseFunc(params["game.pickable.entry_ease"].getValue<std::string>()));

    options.finishFn([this]() {
        can_pick_ = true;
        on_stage_ = true;
      });
  }

  ~PickableCube() {
    // 再生途中のものもあるので、手動で取り除く
    animation_timeline_->removeSelf();
  }
  

  void reserveRotationMove(const int direction, const ci::Vec3i& vector, const int speed) {
    move_direction_ = direction;
    move_vector_    = vector;
    move_speed_     = speed;
  }
  
  bool willRotationMove() {
    return (move_speed_ > 0) && can_pick_ && on_stage_;
  }

  void cancelRotationMove() {
    move_speed_ = 0;
  }

  const ci::Vec3i& moveVector() const { return move_vector_; }
  
  
  void startRotationMove() {
    can_pick_ = false;
    block_position_ += move_vector_;

    auto angle = ci::toRadians(90.0f);
    ci::Quatf rotation_table[] = {
      { ci::Vec3f(1, 0, 0),  angle },
      { ci::Vec3f(1, 0, 0), -angle },
      { ci::Vec3f(0, 0, 1), -angle },
      { ci::Vec3f(0, 0, 1),  angle },
    };

    float duration = rotate_duration_* rotate_speed_rate_[std::min(move_speed_, int(rotate_speed_rate_.size())) - 1];
    
    auto options = animation_timeline_->apply(&move_rotation_,
                                              ci::Quatf::identity(), rotation_table[move_direction_],
                                              duration,
                                              getEaseFunc(rotate_ease_));
    ci::Vec3f pivot_table[] = {
      ci::Vec3f(              0, -cube_size_ / 2,  cube_size_ / 2),
      ci::Vec3f(              0, -cube_size_ / 2, -cube_size_ / 2),
      ci::Vec3f( cube_size_ / 2, -cube_size_ / 2,               0),
      ci::Vec3f(-cube_size_ / 2, -cube_size_ / 2,               0)
    };
    
    auto pivot_rotation = pivot_table[move_direction_];
    auto rotation = rotation_();
    auto position = position_();
    options.updateFn([this, pivot_rotation, rotation, position]() {
        rotation_ = rotation * move_rotation_();

        // 立方体がエッジの部分で回転するよう平行移動を追加
        ci::Matrix44f mat = move_rotation_().toMatrix44();
        auto pivot_pos = mat.transformVec(pivot_rotation);
        position_ = position - pivot_pos + pivot_rotation;
      });
    
    options.finishFn([this]() {
        // 移動後に正確な位置を設定
        // FIXME:回転も正規化
        position_ = ci::Vec3f(block_position_) * cube_size_;
        position_().y += cube_size_;

        EventParam params = {
          { "id", id_ },
          { "block_pos", block_position_ },
        };
        event_.signal("pickable-moved", params);
        can_pick_ = true;
        move_speed_ -= 1;
      });
  }

  void fallFromStage() {
    can_pick_ = false;
    on_stage_ = false;
    
    ci::Vec3f end_value(position_() + ci::Vec3f(0, fall_y_ * cube_size_, 0));
    auto options = animation_timeline_->apply(&position_,
                                              end_value,
                                              fall_duration_,
                                              getEaseFunc(fall_ease_));

    options.finishFn([this]() {
        active_ = false;
      });
  }

  
  u_int id() const { return id_; }
  
  bool isActive() const { return active_; }
  bool canPick() const { return can_pick_; }
  bool isOnStage() const { return on_stage_; }

  int moveSpeed() const { return move_speed_; }
  
  const ci::Vec3f& position() const { return position_(); }
  const ci::Quatf& rotation() const { return rotation_(); }

  const ci::Vec3i& blockPosition() const { return block_position_; }
#if 0
  void blockPosition(const ci::Vec3i& pos) {
    block_position_ = pos;
    
    position_ = ci::Vec3f(pos) * cube_size_;
    position_().y += cube_size_;
  }
#endif
  
  float cubeSize() const { return cube_size_; }
  ci::Vec3f size() const { return ci::Vec3f(cube_size_, cube_size_, cube_size_); }
  const ci::Color& color() const { return color_; }


  // std::findを利用するための定義
  bool operator==(const u_int rhs_id) const {
    return id_ == rhs_id;
  }

  bool operator==(const PickableCube& rhs) const {
    return id_ == rhs.id_;
  }
  
  
private:
  // TIPS:コピー不可
  PickableCube(const PickableCube&) = delete;
  PickableCube& operator=(const PickableCube&) = delete;
  
};

}
