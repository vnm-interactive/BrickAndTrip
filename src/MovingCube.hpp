﻿#pragma once

//
// 適当に動いているおじゃまCube
//

#include <boost/noncopyable.hpp>
#include "cinder/Timeline.h"
#include "cinder/Rand.h"
#include "EasingUtil.hpp"
#include "Utility.hpp"

namespace ngs {

class MovingCube : private boost::noncopyable {
  enum {
    MOVE_NONE = -1,

    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,

    MOVE_MAX
  };


  ci::JsonTree& params_;
  Event<EventParam>& event_;
  
  bool active_;

  u_int id_;
  
  float cube_size_;

  ci::Color color_;

  ci::Vec3i block_position_;
  ci::Vec3i prev_block_position_;
  
  ci::Anim<ci::Vec3f> position_;
  ci::Anim<ci::Quatf> rotation_;

  ci::TimelineRef animation_timeline_;

  // on_stage_  stage上に存在
  // moving_    移動中
  bool on_stage_;
  bool moving_;
  bool can_move_;
  
  int       move_direction_;
  ci::Vec3i move_vector_;
  int       move_speed_;

  float stop_time_;
  
  std::string rotate_ease_;
  float rotate_duration_;
  std::vector<float> rotate_speed_rate_;
  ci::Anim<ci::Quatf> move_rotation_;
  ci::Quatf move_start_rotation_;

  std::string fall_ease_;
  float fall_duration_;
  float fall_y_;

  std::vector<int> move_pattern_;
  size_t current_pattern_;

  
public:
  MovingCube(ci::JsonTree& params,
             ci::TimelineRef timeline,
             Event<EventParam>& event,
             const ci::Vec3i& entry_pos,
             const std::vector<int>& move_pattern) :
    params_(params),
    event_(event),
    active_(true),
    id_(getUniqueNumber()),
    cube_size_(params["game.cube_size"].getValue<float>()),
    color_(Json::getColor<float>(params["game.moving.color"])),
    block_position_(entry_pos),
    prev_block_position_(block_position_),
    rotation_(ci::Quatf::identity()),
    animation_timeline_(ci::Timeline::create()),
    on_stage_(false),
    moving_(false),
    can_move_(false),
    move_direction_(MOVE_NONE),
    move_vector_(ci::Vec3i::zero()),
    move_speed_(1),
    stop_time_(0.0),
    rotate_ease_(params["game.moving.rotate_ease"].getValue<std::string>()),
    rotate_duration_(params["game.moving.rotate_duration"].getValue<float>()),
    move_start_rotation_(rotation_()),
    fall_ease_(params["game.moving.fall_ease"].getValue<std::string>()),
    fall_duration_(params["game.moving.fall_duration"].getValue<float>()),
    fall_y_(params["game.moving.fall_y"].getValue<float>()),
    current_pattern_(0)
  {
    DOUT << "MovingCube()" << std::endl;

    auto current_time = timeline->getCurrentTime();
    animation_timeline_->setStartTime(current_time);
    timeline->apply(animation_timeline_);

    position_ = ci::Vec3f(block_position_) * cube_size_;
    // block_positionが同じ高さなら、StageCubeの上に乗るように位置を調整
    position_().y += cube_size_;

    for (const auto& param : params["game.moving.rotate_speed_rate"]) {
      rotate_speed_rate_.push_back(param.getValue<float>());
    }

    // 移動パターンを2468方式から変換
    std::map<int, int> move_table = {
      { 8, MOVE_UP },
      { 2, MOVE_DOWN },
      { 4, MOVE_LEFT },
      { 6, MOVE_RIGHT },
      { 0, MOVE_NONE }
    };

    for (const auto pat : move_pattern) {
      move_pattern_.push_back(move_table.at(pat));
    }

    // 登場演出
    auto entry_y = Json::getVec2<float>(params["game.moving.entry_y"]);
    float y = ci::randFloat(entry_y.x, entry_y.y) * cube_size_;
    ci::Vec3f start_value(position() + ci::Vec3f(0, y, 0));
    auto options = animation_timeline_->apply(&position_,
                                              start_value, position_(),
                                              params["game.moving.entry_duration"].getValue<float>(),
                                              getEaseFunc(params["game.moving.entry_ease"].getValue<std::string>()));

    options.finishFn([this]() {
        on_stage_ = true;
        
        EventParam params = {
          { "id", id_ },
          { "block_pos", block_position_ },
        };
        event_.signal("moving-on-stage", params);
      });
  }
    
  ~MovingCube() {
    DOUT << "~MovingCube()" << std::endl;

    // 再生途中のものもあるので、手動で取り除く
    animation_timeline_->removeSelf();
  }


  void update(const double progressing_seconds) {
    if (moving_) return;
    
    if (stop_time_ < 0.0f) {
      move_direction_ = move_pattern_[current_pattern_];

      if (move_direction_ == MOVE_NONE) {
        current_pattern_ += 1;
        current_pattern_ %= move_pattern_.size();
        
        stop_time_ = rotate_duration_;
        return;
      }
      
      static ci::Vec3i move_vec[] = {
        {  0, 0,  1 },
        {  0, 0, -1 },
        {  1, 0,  0 },
        { -1, 0,  0 },
      };
      move_vector_ = move_vec[move_direction_];
      can_move_ = true;
    }
    else {
      stop_time_ -= progressing_seconds;
    }
  }

  
  bool willRotationMove() const {
    return !moving_ && on_stage_ && can_move_;
  }

  void removeRotationMoveReserve() {
    stop_time_ = rotate_duration_;
    can_move_  = false;
  }

  void startRotationMove() {
    moving_ = true;

    current_pattern_ += 1;
    current_pattern_ %= move_pattern_.size();

    prev_block_position_ = block_position_;
    block_position_ += move_vector_;

    auto angle = ci::toRadians(90.0f);
    ci::Quatf rotation_table[] = {
      { ci::Vec3f(1, 0, 0),  angle },
      { ci::Vec3f(1, 0, 0), -angle },
      { ci::Vec3f(0, 0, 1), -angle },
      { ci::Vec3f(0, 0, 1),  angle },
    };

    float duration = rotate_duration_ * rotate_speed_rate_[std::min(move_speed_, int(rotate_speed_rate_.size())) - 1];
    
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
        move_start_rotation_ = rotation_;

        moving_    = false;
        stop_time_ = 0.0f;

        EventParam params = {
          { "id", id_ },
          { "block_pos", block_position_ },
        };
        event_.signal("moving-moved", params);
      });
  }

  void fallFromStage() {
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
  bool isOnStage() const { return on_stage_; }
  bool isMoving() const { return moving_; }

  int moveSpeed() const { return move_speed_; }
  const ci::Vec3i& moveVector() const { return move_vector_; }
  
  const ci::Vec3f& position() const { return position_(); }
  const ci::Quatf& rotation() const { return rotation_(); }

  const ci::Vec3i& blockPosition() const { return block_position_; }
  const ci::Vec3i& prevBlockPosition() const { return prev_block_position_; }
  
  float cubeSize() const { return cube_size_; }
  ci::Vec3f size() const { return ci::Vec3f(cube_size_, cube_size_, cube_size_); }

  const ci::Color& color() const { return color_; }


  // std::findを利用するための定義
  bool operator==(const u_int rhs_id) const {
    return id_ == rhs_id;
  }

  bool operator==(const MovingCube& rhs) const {
    return id_ == rhs.id_;
  }
  
};

}