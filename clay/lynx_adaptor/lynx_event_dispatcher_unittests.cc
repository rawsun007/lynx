// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <initializer_list>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "clay/lynx_adaptor/lynx_event_dispatcher.h"
#include "clay/lynx_adaptor/value_converter.h"
#include "clay/public/value.h"
#include "third_party/googletest/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace clay {
namespace {

struct TouchRecord {
  std::string name;
  Value params;
};

class RecordingEngineProxy : public lynx::shell::LynxEngineProxy {
 public:
  bool SendTouchEvent(const std::string& name, const lynx::pub::Value& params,
                      int64_t) override {
    touches.push_back({name, lynx::ValueConverter::CreateClayValue(params)});
    return true;
  }

  bool SendTouchEvent(const std::string& name, int32_t, float, float, float,
                      float, float, float, int64_t,
                      lynx::event::TouchEventTargetPoints) override {
    single_touch_names.push_back(name);
    return true;
  }

  void DispatchTaskToLynxEngine(lynx::base::closure) override {}
  MOCK_METHOD(void, SendCustomEvent,
              (const std::string&, int32_t, const lynx::pub::Value&,
               const std::string&),
              (override));
  void SendGestureEvent(int, int, std::string,
                        const lynx::pub::Value&) override {}
  MOCK_METHOD(void, SendBubbleEvent,
              (const std::string&, int32_t, const lynx::pub::Value&),
              (override));
  void OnPseudoStatusesChanged(
      std::vector<lynx::event::PseudoStatusChange>) override {}
  void StartEventGenerate(const lynx::pub::Value&) override {}
  void StartEventCapture(int64_t) override {}
  void StartEventBubble(int64_t) override {}
  void StartEventFire(bool, int64_t) override {}
  void ScrollByListContainer(int32_t, float, float, float, float) override {}
  void ScrollToPosition(int32_t, int, float, int, bool) override {}
  void ScrollStopped(int32_t) override {}
  int32_t ObtainListChild(int32_t, uint32_t, int64_t, bool) override {
    return -1;
  }
  void RecycleListChild(int32_t, uint32_t) override {}
  void RenderListChild(int32_t, uint32_t, int64_t) override {}
  void UpdateListChild(int32_t, uint32_t, uint32_t, int64_t) override {}
  lynx::tasm::ListData GetListData(int) override { return {}; }
  std::list<int32_t> GetAncestorElements(int32_t) override { return {}; }
  void MarkLayoutDirty(int) override {}
  bool EnableRasterAnimation() override { return false; }
  float GetDensity() const override { return density; }
  void OnFirstMeaningfulPaint() override {}
  void TriggerLayout() override {}

  std::vector<TouchRecord> touches;
  std::vector<std::string> single_touch_names;
  float density = 1.f;
};

class LynxEventDispatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    engine_ = std::make_shared<RecordingEngineProxy>();
    dispatcher_.SetEngineProxy(engine_);
    dispatcher_.SetEnableMultiTouch(true);
  }

  void Touch(const char* name, int pointer, int tag = 10) {
    dispatcher_.OnTouchEvent(name, tag, 10.f, 20.f, 30.f, 40.f, pointer);
  }

  int Identifier(size_t event, int tag) const {
    return engine_->touches.at(event)
        .params.GetMap()
        .at(std::to_string(tag))
        .GetArray()
        .at(0)
        .GetArray()
        .at(0)
        .GetInt();
  }

  LynxEventDispatcher dispatcher_;
  std::shared_ptr<RecordingEngineProxy> engine_;
};

TEST_F(LynxEventDispatcherTest, RecyclesOnlyReleasedIdentifiers) {
  Touch("touchstart", 17);
  Touch("touchstart", 18, 20);
  Touch("touchend", 17);
  Touch("touchmove", 18, 20);
  Touch("touchstart", 19, 30);
  Touch("touchend", 18, 20);
  Touch("touchend", 19, 30);
  Touch("touchstart", 20);

  ASSERT_EQ(engine_->touches.size(), 8u);
  EXPECT_EQ(Identifier(0, 10), 0);
  EXPECT_EQ(Identifier(1, 20), 1);
  EXPECT_EQ(Identifier(2, 10), 0);
  EXPECT_EQ(Identifier(3, 20), 1);
  EXPECT_EQ(Identifier(4, 30), 0);
  EXPECT_EQ(Identifier(5, 20), 1);
  EXPECT_EQ(Identifier(6, 30), 0);
  EXPECT_EQ(Identifier(7, 10), 0);
  const auto& point = engine_->touches.front()
                          .params.GetMap()
                          .at("10")
                          .GetArray()
                          .front()
                          .GetArray();
  EXPECT_DOUBLE_EQ(point.at(3).GetDouble(), 30.);
  EXPECT_DOUBLE_EQ(point.at(4).GetDouble(), 40.);
}

TEST_F(LynxEventDispatcherTest, CancelledStreamDrainsAllPhysicalContacts) {
  Touch("touchstart", 1);
  Touch("touchstart", 2, 20);
  Touch("touchcancel", 1);
  Touch("touchmove", 2, 20);
  Touch("touchstart", 3, 30);
  Touch("touchend", 2, 20);
  Touch("touchmove", 3, 30);
  Touch("touchend", 3, 30);

  ASSERT_EQ(engine_->touches.size(), 3u);
  EXPECT_EQ(engine_->touches.back().name, "touchcancel");
  const auto& targets = engine_->touches.back().params.GetMap();
  ASSERT_EQ(targets.size(), 2u);
  EXPECT_TRUE(targets.at("10").GetArray().empty());
  EXPECT_TRUE(targets.at("20").GetArray().empty());

  Touch("touchstart", 4);
  Touch("touchend", 4);
  ASSERT_EQ(engine_->touches.size(), 5u);
  EXPECT_EQ(engine_->touches.at(3).name, "touchstart");
  EXPECT_EQ(Identifier(3, 10), 0);
  EXPECT_EQ(engine_->touches.at(4).name, "touchend");
  EXPECT_EQ(Identifier(4, 10), 0);
}

TEST_F(LynxEventDispatcherTest,
       CancellationDeduplicatesContactsOnTheSameTarget) {
  Touch("touchstart", 1);
  Touch("touchstart", 2);
  Touch("touchcancel", 1);

  ASSERT_EQ(engine_->touches.size(), 3u);
  const auto& targets = engine_->touches.back().params.GetMap();
  ASSERT_EQ(targets.size(), 1u);
  EXPECT_TRUE(targets.at("10").GetArray().empty());
}

TEST_F(LynxEventDispatcherTest, RepeatedCancellationDoesNotEndTheNextStream) {
  Touch("touchstart", 1);
  Touch("touchstart", 2, 20);
  Touch("touchcancel", 1);
  Touch("touchcancel", 2, 20);
  Touch("touchstart", 3);
  Touch("touchcancel", 1);
  Touch("touchend", 2, 20);
  Touch("touchmove", 3);
  Touch("touchend", 3);

  ASSERT_EQ(engine_->touches.size(), 6u);
  EXPECT_EQ(engine_->touches.at(2).name, "touchcancel");
  EXPECT_EQ(engine_->touches.at(3).name, "touchstart");
  EXPECT_EQ(engine_->touches.at(4).name, "touchmove");
  EXPECT_EQ(engine_->touches.at(5).name, "touchend");
  EXPECT_EQ(Identifier(3, 10), 0);
  EXPECT_EQ(Identifier(4, 10), 0);
  EXPECT_EQ(Identifier(5, 10), 0);
}

TEST_F(LynxEventDispatcherTest, SingleContactCancelRequiresNoFollowingUp) {
  Touch("touchstart", 1);
  Touch("touchcancel", 1);
  Touch("touchstart", 2);
  Touch("touchend", 2);

  ASSERT_EQ(engine_->touches.size(), 4u);
  EXPECT_EQ(engine_->touches.at(1).name, "touchcancel");
  EXPECT_EQ(engine_->touches.at(2).name, "touchstart");
  EXPECT_EQ(Identifier(2, 10), 0);
}

TEST_F(LynxEventDispatcherTest, SyntheticGesturesDoNotRecycleTouchIdentifiers) {
  Touch("touchstart", 1);
  Touch("tap", 1);
  Touch("touchstart", 2, 20);
  Touch("longpress", 2, 20);
  Touch("touchend", 1);
  Touch("touchend", 2, 20);

  ASSERT_EQ(engine_->touches.size(), 4u);
  EXPECT_EQ(Identifier(0, 10), 0);
  EXPECT_EQ(Identifier(1, 20), 1);
  EXPECT_EQ(Identifier(2, 10), 0);
  EXPECT_EQ(Identifier(3, 20), 1);
  ASSERT_EQ(engine_->single_touch_names.size(), 2u);
  EXPECT_EQ(engine_->single_touch_names.at(0), "tap");
  EXPECT_EQ(engine_->single_touch_names.at(1), "longpress");
}

TEST_F(LynxEventDispatcherTest, PageConfigurationResetsCancelledStream) {
  Touch("touchstart", 1);
  Touch("touchstart", 2, 20);
  Touch("touchcancel", 1);
  dispatcher_.SetEnableMultiTouch(true);
  Touch("touchstart", 3);

  ASSERT_EQ(engine_->touches.size(), 4u);
  EXPECT_EQ(engine_->touches.back().name, "touchstart");
  EXPECT_EQ(Identifier(3, 10), 0);
}

TEST_F(LynxEventDispatcherTest, SingleTouchModeUsesTheOriginalInterface) {
  dispatcher_.SetEnableMultiTouch(false);
  Touch("touchstart", 1);
  Touch("touchmove", 1);
  Touch("touchend", 1);

  EXPECT_TRUE(engine_->touches.empty());
  ASSERT_EQ(engine_->single_touch_names.size(), 3u);
  EXPECT_EQ(engine_->single_touch_names.at(0), "touchstart");
  EXPECT_EQ(engine_->single_touch_names.at(1), "touchmove");
  EXPECT_EQ(engine_->single_touch_names.at(2), "touchend");
}

TEST_F(LynxEventDispatcherTest,
       WheelPreservesDeltasAndReportsLogicalCoordinates) {
  engine_->density = 2.f;
  EXPECT_CALL(*engine_, SendBubbleEvent("wheel", 42, ::testing::_))
      .WillOnce(::testing::WithArg<2>([](const lynx::pub::Value& params) {
        const std::initializer_list<std::pair<const char*, double>> expected = {
            {"x", 5},        {"y", 10},       {"pageX", 15},  {"pageY", 20},
            {"clientX", 15}, {"clientY", 20}, {"deltaX", -3}, {"deltaY", 7}};
        for (const auto& [key, number] : expected) {
          const auto value = params.GetValueForKey(key);
          ASSERT_TRUE(value && value->IsNumber()) << key;
          EXPECT_DOUBLE_EQ(value->Number(), number) << key;
        }
      }));

  dispatcher_.OnWheelEvent("wheel", 42, 10.f, 20.f, 30.f, 40.f, -3.f, 7.f);
}

TEST_F(LynxEventDispatcherTest, LayoutChangeForwardsGeometryInDetail) {
  EXPECT_CALL(*engine_,
              SendCustomEvent("layoutchange", 42, ::testing::_, "detail"))
      .WillOnce(::testing::WithArg<2>([](const lynx::pub::Value& params) {
        const auto id = params.GetValueForKey("id");
        ASSERT_TRUE(id && id->IsString());
        EXPECT_EQ(id->str(), "layout-target");
        const auto width = params.GetValueForKey("width");
        ASSERT_TRUE(width && width->IsNumber());
        EXPECT_DOUBLE_EQ(width->Number(), 40);
      }));

  Value::Map detail;
  detail.emplace("id", Value("layout-target"));
  detail.emplace("width", Value(40.f));
  dispatcher_.OnLayoutChanged(42, std::move(detail));
}

TEST_F(LynxEventDispatcherTest, AnimationCancelForwardsKeyframeIdentity) {
  EXPECT_CALL(*engine_,
              SendCustomEvent("animationcancel", 42, ::testing::_, "params"))
      .WillOnce(::testing::WithArg<2>([](const lynx::pub::Value& params) {
        const auto type = params.GetValueForKey("animation_type");
        ASSERT_TRUE(type && type->IsString());
        EXPECT_EQ(type->str(), "keyframe-animation");
        const auto name = params.GetValueForKey("animation_name");
        ASSERT_TRUE(name && name->IsString());
        EXPECT_EQ(name->str(), "fade-out");
      }));

  dispatcher_.OnAnimationEvent("animationcancel", "fade-out", 42);
}

}  // namespace
}  // namespace clay
