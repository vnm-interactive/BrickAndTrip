﻿#pragma once

//
// ゲーム舞台のEntity
//

#include <sstream>
#include <iomanip>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>
#include "cinder/Json.h"
#include "cinder/Timeline.h"
#include "cinder/Rand.h"
#include "Stage.hpp"
#include "Field.hpp"
#include "PickableCube.hpp"
#include "StageItems.hpp"
#include "StageMovingCubes.hpp"
#include "StageSwitches.hpp"
#include "StageOneways.hpp"
#include "StageFallingCubes.hpp"
#include "EventParam.hpp"
#include "Records.hpp"
#include "Bg.hpp"


namespace ngs {

class FieldEntity : private boost::noncopyable {
  enum {
    START_STAGE_NUM = 0,
  };

  
  ci::JsonTree& params_;
  ci::TimelineRef timeline_;
  Event<EventParam>& event_;
  Records& records_;

  std::vector<ci::Color> cube_stage_color_;
  ci::Color cube_line_color_;

  ci::Color stage_color_;
  ci::Color bg_color_;
  std::string light_tween_;
  std::string camera_;

  ci::Color finish_stage_color_;
  ci::Color finish_bg_color_;
  std::string finish_light_tween_;

  float collapse_speed_rate_;
  float collapse_speed_rate_min_;
  float finish_rate_;

  bool  is_continued_;
  float continue_collapse_rate_;

  int total_stage_num_;
  int regular_stage_num_;
  // 再開用情報
  int start_stage_num_;
  int stage_num_;

  int restart_z_;
  
  Stage stage_;

  StageItems items_;
  StageMovingCubes moving_cubes_;
  StageFallingCubes falling_cubes_;
  StageSwitches switches_;
  StageOneways oneways_;

  Bg bg_;
  
  // VS2013には暗黙のmoveコンストラクタが無いのでstd::unique_ptrで保持
  // std::vectorに格納するときに、copyやmoveコンストラクタが呼ばれる
  using PickableCubePtr = std::unique_ptr<PickableCube>;
  std::vector<PickableCubePtr> pickable_cubes_;

  // ステージ開始時の位置(再開用)
  std::vector<ci::Vec2i> start_pickable_entry_;
  
  bool first_started_pickable_;
  bool first_out_pickable_;
  
  int start_line_z_;
  int finish_line_z_;
  int next_start_line_z_;

  int entry_packable_num_;
  
  enum {
    NONE,
    START,
    FINISH,
    CLEAR,
    CLEANUP,
  };
  int mode_;

  bool all_cleard_;
  bool game_aborted_;

  int stage_center_x_;

  ci::TimelineRef event_timeline_;


  struct StageInfo {
    int top_z;
    int bottom_z;
    int entry_num;
    int item_num;
    ci::Color stage_color;
    ci::Color bg_color;
    std::string light_tween;
    std::string camera;
  };
  
  
public:
  FieldEntity(ci::JsonTree& params,
              ci::TimelineRef timeline,
              Event<EventParam>& event,
              Records& records) :
    params_(params),
    timeline_(timeline),
    event_(event),
    records_(records),
    cube_line_color_(Json::getColor<float>(params["game.cube_line_color"])),
    stage_color_(0, 0, 0),
    bg_color_(0, 0, 0),
    finish_stage_color_(0, 0, 0),
    finish_bg_color_(0, 0, 0),
    start_stage_num_(START_STAGE_NUM),
    stage_num_(start_stage_num_),
    restart_z_(0),
    stage_(params, timeline, event),
    items_(params, timeline, event),
    moving_cubes_(params, timeline, event),
    falling_cubes_(params, timeline, event),
    switches_(timeline, event),
    oneways_(timeline, event),
    bg_(params, timeline, event),
    first_started_pickable_(false),
    first_out_pickable_(false),
    collapse_speed_rate_(params["game.collapse_speed_rate"].getValue<float>()),
    collapse_speed_rate_min_(params["game.collapse_speed_rate_min"].getValue<float>()),
    finish_rate_(params["game.finish_rate"].getValue<float>()),
    is_continued_(false),
    continue_collapse_rate_(params["game.continue_collapse_rate"].getValue<float>()),
    total_stage_num_(params["game.total_stage_num"].getValue<int>()),
    regular_stage_num_(params["game.regular_stage_num"].getValue<int>()),
    mode_(NONE),
    all_cleard_(false),
    game_aborted_(false),
    stage_center_x_(0),
    event_timeline_(ci::Timeline::create())
  {
    const auto& colors = params["game.cube_stage_color"];
    for (const auto& color : colors) {
      cube_stage_color_.push_back(Json::getColor<float>(color));
    }

    setupRecords(params);
    
    auto current_time = timeline->getCurrentTime();
    event_timeline_->setStartTime(current_time);
    timeline->apply(event_timeline_);

#ifdef DEBUG
    start_stage_num_ = params_["game.start_stage"].getValue<int>();
#endif
  }

  ~FieldEntity() {
    // 再生途中のものもあるので、手動で取り除く
    event_timeline_->removeSelf();
  }


  void update(const double progressing_seconds) noexcept {
    records_.progressPlayTimeCurrntGame(progressing_seconds);

    items_.update(progressing_seconds, stage_);
    moving_cubes_.update(progressing_seconds,
                         stage_, gatherPickableCubePosition());
    falling_cubes_.update(progressing_seconds, stage_);
    switches_.update(progressing_seconds, stage_);
    oneways_.update(progressing_seconds, stage_);

    // Pickableの死亡判定
    decideEachPickableCubeFalling();
    decideEachPickableCubeAlive();
    
    boost::remove_erase_if(pickable_cubes_,
                           [](const PickableCubePtr& cube) {
                             return !cube->isActive();
                           });

    switch (mode_) {
    case START:
      // PickableCubeのstart判定
      if (isPickableCubeStarted()) {
        mode_ = FINISH;
        first_started_pickable_ = true;

        EventParam params = {
          { "stage_color", stage_color_ },
          { "bg_color", bg_color_ },
          { "light_tween", light_tween_ },
        };
        event_.signal("first-pickable-started", params);
      }
      break;

    case FINISH:
      // 全PickableCubeのfinish判定
      // FIXME:event処理で判定できる
      if (isAllPickableCubesFinished()) {
        controlFinishedPickableCubes();
        
        mode_ = CLEAR;
        event_.signal("all-pickable-finished", EventParam());
        {
          EventParam params = {
            { "stage_color", finish_stage_color_ },
            { "bg_color",    finish_bg_color_ },
            { "light_tween", finish_light_tween_ },
          };
          event_.signal("stage-color", params);
        }
      }
      break;

    case CLEAR:
      // finish後、stageの生成と崩壊の完了判定
      if (stage_.isFinishedBuildAndCollapse()) {
        mode_ = NONE;
        event_.signal("stage-cleared", EventParam());
      }
      break;

    case CLEANUP:
      // gameover後、stage崩壊の完了判定
      if (stage_.isFinishedCollapse() && !isPickableCubeOnStage()) {
        mode_ = NONE;

        EventParam params = {
          { "all_cleard",   all_cleard_ },
          { "game_aborted", game_aborted_ },
        };
        event_.signal("stage-all-collapsed", params);
      }
      break;
    }

    // Finish判定の後に、移動判定を行わないと、
    // 「Finishした直後にFinish-Lineを割って落下」が起きる
    decideEachPickableCubeMoving();
    setAdjoinOtherPickableCube();

#if 0
    // 全PickableCubeの落下判定は、毎フレーム判定を避けている
    if (did_fall && !isPickableCubeOnStage()) {
      event_.signal("fall-all-pickable", EventParam());
    }
#endif

    switch (mode_) {
    case START:
    case FINISH:
      {
        const auto& current_stage = records_.currentStage();
      
        EventParam params = {
          { "play-time", current_stage.play_time },
        };
        event_.signal("update-record", params);
      }
      break;
    }

    {
      // 背景で使う中央座標を算出
      ci::Vec3i pos(stage_center_x_, 0, stage_.getActiveBottomZ());
      bg_.setCenterPosition(pos);
    }
    bg_.update(progressing_seconds);
  }


  // 最初のStageとStartLine、PickableCubeを準備
  void setupStartStage() noexcept {
    auto stage_info = addCubeStage("startline.json");
    next_start_line_z_ = stage_info.top_z - 1;

    // bgの位置を決めるためにステージの中央座標を求める
    const auto& width = stage_.getStageWidth();
    stage_center_x_ = (width.x + width.y) / 2;

    {
      EventParam params = {
        { "stage_color", stage_info.stage_color },
        { "bg_color",    stage_info.bg_color },
        { "light_tween", stage_info.light_tween }
      };
      event_.signal("stage-color", params);
    }
    
    stage_.startBuildStage(1.0f, false);

    stage_num_    = start_stage_num_;
    all_cleard_   = false;
    game_aborted_ = false;

    if (is_continued_) {
      // Game再開時は前回の位置を再現
      entryContinuedPickableCube(stage_info.bottom_z);
    }
    else {
      // entryするpickableは、直前のステージまでの合算
      int entry_packable_num = calcEntryPickableCube(stage_num_);
      
      ci::Vec2i entry_pos = Json::getVec2<int>(params_["game.pickable.entry_pos"]);
      // entry_pos.y += stage_info.bottom_z;
      float delay = params_["game.pickable.entry_start_delay"].getValue<float>();
      
      // 2個目以降はrandom
      bool entry_random = false;
      for (int i = 0; i < entry_packable_num; ++i) {
        entryPickableCube(entry_pos, stage_info.bottom_z,
                          delay, entry_random, false);
        entry_random = true;
      }
    }
    // entry時に登録されるので、再開用の位置は一旦初期化
    start_pickable_entry_.clear();
    
    if (!is_continued_) {
      EventParam params = {
        { "name", stage_info.camera },
      };
      event_.signal("camera-change", params);
    }

    records_.prepareGameRecord();
  }

  // Stageの全Buildを始める
  void startStageBuild() noexcept {
    stage_.openStartLine();
    items_.clear();
    moving_cubes_.clear();
    falling_cubes_.clear();
    switches_.clear();
    oneways_.clear();
    
    auto stage_info     = addCubeStage(getStagePath(stage_num_));
    finish_line_z_      = stage_info.top_z;
    entry_packable_num_ = stage_info.entry_num;
    int entry_item_num  = stage_info.item_num;

    stage_.setFinishLine(finish_line_z_);
    
    stage_color_ = stage_info.stage_color;
    bg_color_    = stage_info.bg_color;
    light_tween_ = stage_info.light_tween;
    camera_      = stage_info.camera;

    // bgの位置を決めるためにステージの中央座標を求める
    const auto& width = stage_.getStageWidth();
    stage_center_x_   = (width.x + width.y) / 2;

    start_line_z_      = next_start_line_z_;
    stage_info         = addCubeStage("finishline.json");
    next_start_line_z_ = stage_info.top_z - 1;

    finish_stage_color_ = stage_info.stage_color;
    finish_bg_color_    = stage_info.bg_color;
    finish_light_tween_ = stage_info.light_tween;

    mode_ = START;
    first_started_pickable_ = false;
    first_out_pickable_     = false;
    
    records_.prepareCurrentGameRecord(stage_num_,
                                      finish_line_z_ - start_line_z_ + 1,
                                      stage_.buildSpeed(),
                                      event_timeline_->getCurrentTime(),
                                      entry_item_num);

    stage_num_ += 1;

    stage_.startBuildStage(1.0f, true);
    float collapse_rate = is_continued_ ? continue_collapse_rate_
                                        : 1.0f;
    stage_.setupAutoCollapse(finish_line_z_ - 1, collapse_rate);

    {
      EventParam params = {
        { "name", camera_ },
      };
      event_.signal("camera-change", params);
    }
    requestSound(event_, "build-start");
    
    DOUT << "Continue game:" << is_continued_ << std::endl;
  }

  // StageのFinishLineまでの崩壊を始める
  void startStageCollapse() noexcept {
    if (!stage_.isStartedCollapse()) {
      stage_.startCollapseStage(finish_line_z_ - 1);
    }
  }
  
  // FinishLineまで一気に崩壊 && FinishLineを一気に生成
  void completeBuildAndCollapseStage() noexcept {
    stage_.stopBuildAndCollapse();
    stage_.startBuildStage(finish_rate_, false);
    stage_.startCollapseStage(finish_line_z_ - 1, finish_rate_);

    records_.storeStageRecord();

    // 全ステージクリア判定
    bool all_cleard    = false;
    bool regular_stage = false;
    bool all_stage     = false;

    if (stage_num_ == regular_stage_num_) {
      // 11stageが登場するか判定
      if (!records_.isSatisfyRegularStageRank()) {
        all_cleard    = true;
        regular_stage = true;
        records_.cleardRegularStages();
      }
    }
    else if (stage_num_ == total_stage_num_) {
      all_cleard = true;
      all_stage  = true;
      records_.cleardAllStages();
    }
    all_cleard_ = all_cleard;
    
    records_.write(params_["game.records"].getValue<std::string>());

    {
      const auto& current_game  = records_.currentGame();
      const auto& current_stage = records_.currentStage();

      // 次のステージ or 全ステージクリア
      EventParam params = {
        { "clear_time",     current_stage.play_time },
        { "item_num",       current_stage.item_num },
        { "item_total_num", current_stage.item_total_num },
        { "complete_item",  current_stage.complete_item },
        { "score",          current_stage.score },
        { "rank",           current_stage.rank },
        { "highest_score",  current_stage.highest_score },
        { "highest_rank",   current_stage.highest_rank },
        { "all_cleared",    all_cleard },
        { "regular_stage",  regular_stage },
        { "all_stage",      all_stage },
        
        { "total_score",         current_game.score },
        { "highest_total_score", current_game.highest_score },
        { "play_item_num",       current_game.item_num },
        { "play_item_total_num", current_game.item_total_num },
        { "highest_item_num",    current_game.highest_item_num },

        { "current_stage", stage_num_ },
      };

      event_.signal("begin-stageclear", params);
    }
    
    // sleep中のPickableCubeを起こす
    for (auto& cube : pickable_cubes_) {
      if (cube->isSleep()) {
        cube->awaken();
        cube->endSleepingColor();
      }
    }
  }

  // GameOver時の処理
  void gameover() noexcept {
    setRestartLine();
    
    stopBuildAndCollapse();
    records_.storeRecord();
    records_.write(params_["game.records"].getValue<std::string>());

    {
      const auto& current_game  = records_.currentGame();

      EventParam params = {
        { "score",        current_game.score },
        { "hi_score",     current_game.highest_score },
        { "can_continue", canContinue() },
      };
      event_.signal("begin-gameover", params);
    }
  }

  // GameOver時などで生成&崩壊を止める
  void stopBuildAndCollapse() noexcept {
    stage_.stopBuildAndCollapse();
    mode_ = NONE;
  }

  // 中断
  void abortGame() noexcept {
    setRestartLine();
    
    game_aborted_ = true;
    records_.disableRecordCurrentGame();
    cleanupField();
  }

  // 強制崩壊
  void collapseStage() noexcept {
    stage_.startCollapseStage(next_start_line_z_);

    first_started_pickable_ = true;
    first_out_pickable_     = true;

    mode_ = NONE;
  }
  
  // リスタート前のClean-up
  void cleanupField(const bool continue_game = false) noexcept {
    event_timeline_->clear();
    items_.cleanup();
    moving_cubes_.cleanup();
    falling_cubes_.cleanup();

    items_.clear();
    moving_cubes_.clear();
    falling_cubes_.clear();
    switches_.clear();
    oneways_.clear();
    
    first_started_pickable_ = true;
    first_out_pickable_     = true;

    stage_.stopBuildAndCollapse();
    stage_.startCollapseStage(next_start_line_z_, finish_rate_);
    mode_ = CLEANUP;
    
    requestSound(event_, "stage-collapse");

    // 再開用情報
    start_stage_num_ = continue_game ? (stage_num_ - 1)
#ifdef DEBUG
                                     : params_["game.start_stage"].getValue<int>();
#else
                                     : START_STAGE_NUM;
#endif
    records_.continuedGame(continue_game);
    is_continued_ = continue_game;
  }
  
  void restart() noexcept {
    stage_.cleanup();
    stage_.restart(restart_z_);
    mode_ = NONE;
  }


  void pickPickableCube(const u_int id) noexcept {
    auto it = findPickableCube(id);
    assert(it);
    auto& cube = *it;

    cube->startPickingColor();
  }

  void movePickableCube(const u_int id, const int direction, const int speed) noexcept {
    auto it = findPickableCube(id);
    assert(it);
    auto& cube = *it;

    cube->endPickingColor();

    if ((direction == PickableCube::MOVE_NONE) || !speed) {
      cube->cancelRotationMove();
      return;
    }

    
    // 移動可能かStageを調べる
    static ci::Vec3i move_vec[] = {
      {  0, 0,  1 },
      {  0, 0, -1 },
      {  1, 0,  0 },
      { -1, 0,  0 },
    };

    const auto& move_vector = move_vec[direction];
    auto moved_pos = cube->blockPosition() + move_vector;
    if (canPickableCubeMove(cube, moved_pos)) {
      cube->reserveRotationMove(direction, move_vector, speed);
    }
  }

  void cancelPickPickableCubes() noexcept {
    for (auto& cube : pickable_cubes_) {
      cube->endPickingColor();
    }
  }

  void movedPickableCube(const u_int id, const ci::Vec3i& block_pos) noexcept {
    // switch踏んだ処理
    const auto* const targets = switches_.startSwitch(block_pos);
    if (targets) {
      startSwitchTargets(*targets);
      requestSound(event_, "switch");
      return;
    }

    auto oneway = oneways_.startOneway(block_pos);
    if (oneway.first != Oneway::NONE) {
      static std::map<int, int> direction = {
        { Oneway::UP,    PickableCube::MOVE_UP },
        { Oneway::DOWN,  PickableCube::MOVE_DOWN },
        { Oneway::LEFT,  PickableCube::MOVE_LEFT },
        { Oneway::RIGHT, PickableCube::MOVE_RIGHT },
      };
      
      movePickableCube(id, direction.at(oneway.first), oneway.second);
      requestSound(event_, "oneway");
      
      return;
    }
  }

  // Ttile画面での画面遷移とゲーム開始操作が被るので用意した
  void enablePickableCubeMovedEvent(const bool enable = true) noexcept {
    for (auto& cube : pickable_cubes_) {
      cube->enableMovedEvent(enable);
    }
  }
  
  // finish-line上のPickableCubeを生成
  void entryPickableCubes() noexcept {
    // 再開用に位置を保存
    storePickableCubePosition(finish_line_z_);
    
    if (entry_packable_num_ > 0) {
      // Finish lineの次が(z = 0)として生成
      ci::Vec2i entry_pos = Json::getVec2<int>(params_["game.pickable.entry_pos"]);
      // entry_pos.y += finish_line_z_;
      float delay = params_["game.pickable.entry_next_delay"].getValue<float>();
      for (int i = 0; i < entry_packable_num_; ++i) {
        entryPickableCube(entry_pos, finish_line_z_, delay, true, false);
      }
    }
  }

  
#ifdef DEBUG
  // bottom lineに１つ召喚
  void entryPickableCube() noexcept {
    ci::Vec2i entry_pos = Json::getVec2<int>(params_["game.pickable.entry_pos"]);
    // entry_pos.y += stage_.getActiveBottomZ();
    
    entryPickableCube(entry_pos, stage_.getActiveBottomZ(),
                      0.0f, true, false);
  }
#endif

  void entryStageObjects(const int active_z) noexcept {
    items_.entryItemCube(active_z);
    moving_cubes_.entryCube(active_z);
    falling_cubes_.entryCube(active_z);
    switches_.entrySwitches(active_z);
    oneways_.entryOneways(active_z);
  }

  void pickupedItemCube() noexcept {
    records_.increaseItemNumCurrentGame();
  }

  // PickableCubeのIdle
  void startIdlePickableCube(const u_int id) noexcept {
    auto it = findPickableCube(id);
    if (!it) return;

    auto& cube = *it;
    const ci::Vec3i block_pos = cube->blockPosition();

    std::vector<int> directions;

    static ci::Vec3i move_vec[] = {
      {  0, 0,  1 },
      {  0, 0, -1 },
      {  1, 0,  0 },
      { -1, 0,  0 },
    };
    
    for (u_int i = 0; i < elemsof(move_vec); ++i) {
      if (!isPickableCube(block_pos + move_vec[i])) {
        directions.push_back(i);
      }
    }

    if (directions.empty()) return;

    cube->startIdleMotion(directions);
  }

  void enableRecordPlay() noexcept {
    records_.enableRecordCurrentGame();
  }

  // すべてのPickableCubeを昇天
  void riseAllPickableCube() noexcept {
    for (auto& cube : pickable_cubes_) {
      cube->rise();
    }
  }
  
  // 現在のFieldの状態を作成
  Field fieldData() noexcept {
#ifdef DEBUG
    auto bg_bbox = bg_.getBbox();
#endif
    
    Field field = {
      stage_.activeCubes(),
      stage_.collapseCubes(),
      pickable_cubes_,
      items_.items(),
      moving_cubes_.cubes(),
      falling_cubes_.cubes(),
      switches_.switches(),
      oneways_.oneways(),
      bg_.cubes(),

#ifdef DEBUG
      bg_bbox.first,
      bg_bbox.second,
#endif
    };

    return field;
  }

  bool isContinuedGame() const noexcept {
    return records_.isContinuedGame();
  }

  int getStageTopZ() const noexcept {
    return stage_.getTopZ();
  }
  
  void setRestartLine() noexcept {
    restart_z_ = std::max(stage_.getActiveBottomZ() - 5, 0);
  }
  
  
private:
  // TODO:参照の無効値をあらわすためにboost::optionalを利用
  boost::optional<PickableCubePtr&> findPickableCube(const u_int id) noexcept {
    auto it = std::find_if(std::begin(pickable_cubes_), std::end(pickable_cubes_),
                           [id](const PickableCubePtr& obj) {
                             return *obj == id;
                           });

    if (it == std::end(pickable_cubes_)) return boost::optional<PickableCubePtr&>();
    
    return boost::optional<PickableCubePtr&>(*it);
  }


  void entryPickableCube(const ci::Vec2i& entry_pos,
                         const int offset_z,
                         const float delay,
                         const bool random, const bool sleep) noexcept {
    event_timeline_->add([this, entry_pos, offset_z, random, sleep]() {
        const auto& stage_width = stage_.getStageWidth();
        int entry_y = entry_pos.y + offset_z;
        while (1) {
          // 何度か試してみて、ダメなら登場Z位置を変えて試す
          for (int i = 0; i < 10; ++i) {
            int x = random ? ci::randInt(stage_width.x + 1, stage_width.y - 1)
                           : entry_pos.x;
            
            auto pos = ci::Vec3i(x, 0, entry_y);
            if (isPickableCube(pos)) continue;
          
            auto cube = PickableCubePtr(new PickableCube(params_, timeline_, event_, pos,
                                                         (mode_ == CLEAR) ? false : sleep));
            pickable_cubes_.push_back(std::move(cube));

            // 再開用の位置を保存
            start_pickable_entry_.push_back(ci::Vec2i(pos.x, pos.z - offset_z));
            return;
          }
          entry_y += 1;
        }
        
      }, event_timeline_->getCurrentTime() + delay);
  }

  void entryContinuedPickableCube(const int offset_z) noexcept {
    assert((start_pickable_entry_.size() > 0) && "there is no continued PickableCube.");

    float delay = params_["game.pickable.entry_start_delay"].getValue<float>();

    for (const auto& entry_pos : start_pickable_entry_) {
      entryPickableCube(entry_pos, offset_z,
                        delay, false, false);
    }
  }

  void storePickableCubePosition(const int offset_z) noexcept {
    start_pickable_entry_.clear();

    for (const auto& cube : pickable_cubes_) {
      if (!cube->isOnStage()) continue;
      
      const auto& pos = cube->blockPosition();
      start_pickable_entry_.push_back(ci::Vec2i(pos.x, pos.z - offset_z));
    }
  }

  
  StageInfo addCubeStage(const std::string& path) noexcept {
    auto stage = Json::readFromFile(path);
    int current_z = stage_.getTopZ();

    int x_offset = Json::getValue(stage, "x_offset", 0);
    int top_z = stage_.addCubes(stage,
                                x_offset,
                                cube_stage_color_, cube_line_color_);

    int entry_num = Json::getValue(stage, "pickable", 0);

    int item_num = items_.addItemCubes(stage, current_z, x_offset);
    moving_cubes_.addCubes(stage, current_z, x_offset);
    falling_cubes_.addCubes(stage, current_z, x_offset);
    switches_.addSwitches(params_, stage, current_z, x_offset);
    oneways_.addOneways(params_, stage, current_z, x_offset);

    StageInfo info = {
      top_z,
      current_z,
      entry_num,
      item_num,
      Json::getColor<float>(stage["color"]),
      Json::getColor<float>(stage["bg_color"]),
      Json::getValue(stage, "light_tween", std::string("normal")),
      Json::getValue(stage, "camera", std::string("normal")),
    };
    
    return info;
  }


  // 全てのPickableCubeの移動開始
  void decideEachPickableCubeMoving() noexcept {
    for (auto& cube : pickable_cubes_) {
      if (cube->willRotationMove()) {
        if (canPickableCubeMove(cube, cube->blockPosition() + cube->moveVector())) {
          cube->startRotationMove();
          // 回転開始時にitem pickup判定とか
          pickupStageItems(cube);
        }
        else {
          cube->cancelRotationMove();
        }
      }
    }
  }

  // PickableCubeが隣接するか判定
  void setAdjoinOtherPickableCube() noexcept {
    for (auto& cube : pickable_cubes_) {
      static ci::Vec3i offset_table[] = {
        {  1, 0,  0 },
        { -1, 0,  0 },
        {  0, 0,  1 },
        {  0, 0, -1 },
      };

      bool adjoint = false;
      const auto pos = cube->blockPosition();
      for (const auto& offset : offset_table) {
        if (isPickableCube(pos + offset)) {
          adjoint = true;
          break;
        }
      }

      cube->setAdjoinOther(adjoint);
    }
  }

  void pickupStageItems(const PickableCubePtr& cube) noexcept {
    auto result = items_.canGetItemCube(cube->blockPosition());
    if (result.first) {
      items_.pickupItemCube(result.second);
    }
  }

  bool canPickableCubeMove(const PickableCubePtr& cube, const ci::Vec3i& block_pos) const noexcept {
    // 移動先に他のPickableCubeがいたら移動できない
    for (const auto& other_cube : pickable_cubes_) {
      // 自分自身との判定はスキップ
      if (*cube == *other_cube) continue;

      if (block_pos == other_cube->blockPosition()) return false;

      // 相手が移動中の場合は直前の位置もダメ
      if (other_cube->isMoving()
          && (block_pos == other_cube->prevBlockPosition())) {
        return false;
      }
    }

    // 移動先にMovingCubeがあってもダメ
    if (moving_cubes_.isCubeExists(block_pos)) return false;

    // 移動先にFallingCubeがあってもダメ
    if (falling_cubes_.isCubeExists(block_pos)) return false;
    
    // 移動先の高さが同じじゃないと移動できない
    auto height = stage_.getStageHeight(block_pos);

    return height.first && (height.second == block_pos.y);
  }

  bool isPickableCube(const ci::Vec3i& block_pos) const noexcept {
    for (auto& cube : pickable_cubes_) {
      const auto& pos = cube->blockPosition();
      
      if ((pos.x == block_pos.x) && (pos.z == block_pos.z)) {
        return true;
      }
    }
    return false;
  }
  
  // PickableCubeの落下判定
  void decideEachPickableCubeFalling() noexcept {
    for (auto& cube : pickable_cubes_) {
      if (!cube->isOnStage() || cube->isMoving()) continue;
      
      auto height = stage_.getStageHeight(cube->blockPosition());
      if (!height.first) {
        cube->endPickingColor();
        cube->fallFromStage();

        if (!cube->isSleep()) {
          EventParam params = {
            { "id", cube->id() },
          };
          event_.signal("fall-pickable", params);

          if (!first_out_pickable_) {
            first_out_pickable_ = true;
            event_.signal("first-out-pickable", params);
          }
        }
      }
    }
  }

  // ドッスンに踏まれてないか判定
  void decideEachPickableCubeAlive() noexcept {
    for (auto& cube : pickable_cubes_) {
      if (!cube->isOnStage() || cube->isPressed()) continue;

      if (falling_cubes_.isCubePressed(cube->blockPosition())) {
        // 踏まれた!!
        cube->pressed();
        
        EventParam params = {
          { "id", cube->id() },
          { "block_position", cube->blockPosition() }
        };
        
        event_.signal("pressed-pickable", params);

        if (!first_out_pickable_) {
          first_out_pickable_ = true;
          event_.signal("first-out-pickable", params);
        }
      }
    }
  }

  
  // １つでもPickableCubeがStage上にいるか判定
  bool isPickableCubeOnStage() const noexcept {
    for (auto& cube : pickable_cubes_) {
      // sleep中や潰されたのは勘定しない
      if (cube->isSleep() || cube->isPressed()) continue;
      
      if (cube->isOnStage()) {
        return true;
      }
    }
    
    return false;
  }

  
  bool isPickableCubeStarted() const noexcept {
    if (first_started_pickable_ || pickable_cubes_.empty()) return false;

    for (const auto& cube : pickable_cubes_) {
      if (cube->isSleep()) continue;

      // 移動中の場合は移動前の位置で判定
      const auto& position = cube->isMoving() ? cube->prevBlockPosition()
                                              : cube->blockPosition();
      
      if (position.z >= start_line_z_) {
        return true;
      }
    }
    return false;
  }


  std::vector<ci::Vec3i> gatherPickableCubePosition() const noexcept {
    std::vector<ci::Vec3i> pos;
    
    for (const auto& cube : pickable_cubes_) {
      pos.push_back(cube->blockPosition());
      if (cube->isMoving()) {
        pos.push_back(cube->prevBlockPosition());
      }
    }
    return pos;
  }
  
#if 0
  // すべてのPickableCubeがStartしたか判定
  bool isAllPickableCubesStarted() noexcept {
    if (pickable_cubes_.empty()) return false;

    bool started = true;
    for (const auto& cube : pickable_cubes_) {
      if (!cube->canPick()) {
        // FIXME:操作できないPickableCubeがあったらfalse
        started = false;
        break;
      }

      if (cube->blockPosition().z < start_line_z_) {
        // StartできていないPickableCubeが1つでもあればfalse
        started = false;
        break;
      }
    }
    
    return started;
  }
#endif

  // すべてのPickableCubeがFinishしたか判定
  bool isAllPickableCubesFinished() noexcept {
    if (pickable_cubes_.empty()) return false;

    bool finished = true;
    for (const auto& cube : pickable_cubes_) {
      if (cube->isSleep()) continue;
      
      if (!cube->isOnStage() || cube->isPressed()) {
        finished = false;
        break;
      }

      int z = cube->blockPosition().z;
      if (cube->isMoving()) {
        z = std::min(cube->prevBlockPosition().z, z);
      }
      
      if (z < finish_line_z_) {
        // FinishできていないPickableCubeが1つでもあればfalse
        finished = false;
        break;
      }
    }
    
    return finished;
  }

  // Finish時、ラインをまたいで戻ろうとするPickableの動きを止める
  void controlFinishedPickableCubes() {
    for (auto& cube : pickable_cubes_) {
      if (cube->isSleep()) continue;

      if (!cube->isOnStage() || cube->isPressed()) {
        continue;
      }
      
      cube->controlFinishedMove();
    }
  }

  int getPickableCubeTopZ() const noexcept {
    int top_z = 0;
    for (const auto& cube : pickable_cubes_) {
      top_z = std::max(cube->blockPosition().z, top_z);
    }

    return top_z;
  }

#if 0
  // stageの生成速度を調整
  void controlStageBuildSpeed() noexcept {
    int active_top_z = stage_.getActiveTopZ();
    int pickable_top_z = getPickableCubeTopZ();
    
    float speed_rate = std::min((active_top_z - pickable_top_z) * collapse_speed_rate_, 1.0f);
    speed_rate = std::max(speed_rate, collapse_speed_rate_min_);

    DOUT << speed_rate << std::endl;
    
    stage_.setBuildSpeedRate(speed_rate);
  }
#endif


  void startSwitchTargets(const std::vector<ci::Vec3i>& targets) noexcept {
    for (const auto& target : targets) {
      stage_.moveStageCube(target);
      moving_cubes_.moveCube(target);
      items_.moveCube(target);
    }
  }

  bool canContinue() const noexcept {
    // 最終ステージはcontinue不可
    // if (stage_num_ == total_stage_num_) return false;
    
    // stage開始時にstage_num_は加算されている
#ifdef DEBUG
    return stage_num_ != (params_["game.start_stage"].getValue<int>() + 1);
#else
    return stage_num_ != 1;
#endif
  }

  
  void setupRecords(const ci::JsonTree& params) noexcept {
    records_.setStageNum(params["game.regular_stage_num"].getValue<size_t>(),
                         params["game.total_stage_num"].getValue<size_t>());
    
    records_.setScoreInfo(params["game.score.clear_time_score"].getValue<int>(),
                          params["game.score.clear_time_score_rate"].getValue<float>(),
                          params["game.score.item_score"].getValue<int>(),
                          params["game.score.item_perfect_score"].getValue<int>(),
                          params["game.score.stage_collect"].getValue<float>(),
                          Json::getArray<int>(params["game.score.rank_rate_table"]));

    int regular_item_num = 0;
    int all_item_num     = 0;
    {
      int i = 0;
      {
        int stage_num = params["game.regular_stage_num"].getValue<int>();
        for (; i < stage_num; ++i) {
          regular_item_num += getStageItemNum(i);
        }
      }

      {
        int stage_num = params["game.total_stage_num"].getValue<int>();
        // 全ステージの合計を求めるために、regular以降のステージの合計を求めている
        for (; i < stage_num; ++i) {
          all_item_num += getStageItemNum(i);
        }
        all_item_num += regular_item_num;
      }
    }
    records_.setItemNum(regular_item_num, all_item_num);
    
    DOUT << "regular items:" << regular_item_num
         << " all items:" << all_item_num
         << std::endl;

    {
      int rank = params["game.satisfy_rank"].getValue<int>();
      records_.setSatisfyRank(rank);

#ifdef DEBUG
      auto& rank_text = params["stageclear.rank"];
      DOUT << "satisfy rank:" << rank_text[rank].getValue<std::string>() << std::endl;
#endif
    }
  }

  std::string getStagePath(const int stage_num) noexcept {
    return params_["game.stage_path"][stage_num].getValue<std::string>();
  }

  int getStageItemNum(const int stage_index) noexcept {
    auto path  = getStagePath(stage_index);
    auto stage = Json::readFromFile(path);

    if (!stage.hasChild("items")) return 0;
          
    return int(stage["items"].getNumChildren());
  }
  
  int calcEntryPickableCube(const int stage_num) noexcept {
    int entry_num = getPickableCubeEntryNum("startline.json");
    for (int i = 0; i < stage_num; ++i) {
      entry_num += getPickableCubeEntryNum(getStagePath(i));
    }
    return entry_num;
  }
  
  static int getPickableCubeEntryNum(const std::string& path) noexcept {
    auto stage = Json::readFromFile(path);
    return Json::getValue(stage, "pickable", 0);
  }
  
};

}
