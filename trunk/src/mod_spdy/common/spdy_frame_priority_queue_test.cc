// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mod_spdy/common/spdy_frame_priority_queue.h"

#include "base/time.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

// spdy_protocol.h defines only SPDY_PRIORITY_LOWEST and HIGHEST.  Define the
// others here:
#define SPDY_PRIORITY_MIDHIGH 1
#define SPDY_PRIORITY_MIDLOW 2

namespace {

spdy::SpdyStreamId LastStreamId(spdy::SpdyFrame* frame) {
  if (!frame->is_control_frame() ||
      static_cast<spdy::SpdyControlFrame*>(frame)->type() != spdy::GOAWAY) {
    ADD_FAILURE() << "Frame is not a GOAWAY frame.";
    return 0;
  }
  return static_cast<spdy::SpdyGoAwayControlFrame*>(frame)->
      last_accepted_stream_id();
}

void ExpectPop(spdy::SpdyStreamId expected,
               mod_spdy::SpdyFramePriorityQueue* queue) {
  spdy::SpdyFrame* raw_frame = NULL;
  const bool success = queue->Pop(&raw_frame);
  scoped_ptr<spdy::SpdyFrame> scoped_frame(raw_frame);
  EXPECT_TRUE(success);
  ASSERT_TRUE(scoped_frame != NULL);
  ASSERT_EQ(expected, LastStreamId(scoped_frame.get()));
}

void ExpectEmpty(mod_spdy::SpdyFramePriorityQueue* queue) {
  spdy::SpdyFrame* frame = NULL;
  EXPECT_FALSE(queue->Pop(&frame));
  EXPECT_TRUE(frame == NULL);
}

TEST(SpdyFramePriorityQueueTest, Simple) {
  mod_spdy::SpdyFramePriorityQueue queue;
  ExpectEmpty(&queue);

  queue.Insert(SPDY_PRIORITY_LOWEST, spdy::SpdyFramer::CreateGoAway(4));
  queue.Insert(SPDY_PRIORITY_HIGHEST, spdy::SpdyFramer::CreateGoAway(1));
  queue.Insert(SPDY_PRIORITY_LOWEST, spdy::SpdyFramer::CreateGoAway(3));

  ExpectPop(1, &queue);
  ExpectPop(4, &queue);

  queue.Insert(SPDY_PRIORITY_MIDLOW, spdy::SpdyFramer::CreateGoAway(2));
  queue.Insert(SPDY_PRIORITY_MIDHIGH, spdy::SpdyFramer::CreateGoAway(6));
  queue.Insert(SPDY_PRIORITY_MIDHIGH, spdy::SpdyFramer::CreateGoAway(5));

  ExpectPop(6, &queue);
  ExpectPop(5, &queue);
  ExpectPop(2, &queue);
  ExpectPop(3, &queue);
  ExpectEmpty(&queue);
}

TEST(SpdyFramePriorityQueueTest, InsertFront) {
  mod_spdy::SpdyFramePriorityQueue queue;
  ExpectEmpty(&queue);

  queue.Insert(SPDY_PRIORITY_LOWEST, spdy::SpdyFramer::CreateGoAway(4));
  queue.InsertFront(spdy::SpdyFramer::CreateGoAway(2));
  queue.InsertFront(spdy::SpdyFramer::CreateGoAway(6));
  queue.Insert(SPDY_PRIORITY_HIGHEST, spdy::SpdyFramer::CreateGoAway(1));
  queue.Insert(SPDY_PRIORITY_LOWEST, spdy::SpdyFramer::CreateGoAway(3));

  ExpectPop(6, &queue);
  ExpectPop(2, &queue);
  ExpectPop(1, &queue);
  ExpectPop(4, &queue);

  queue.InsertFront(spdy::SpdyFramer::CreateGoAway(5));

  ExpectPop(5, &queue);
  ExpectPop(3, &queue);
  ExpectEmpty(&queue);
}

TEST(SpdyFramePriorityQueueTest, BlockingPop) {
  mod_spdy::SpdyFramePriorityQueue queue;
  spdy::SpdyFrame* frame;
  ASSERT_FALSE(queue.Pop(&frame));

  const base::TimeDelta time_to_wait = base::TimeDelta::FromMilliseconds(50);
  const base::TimeTicks start = base::TimeTicks::HighResNow();
  ASSERT_FALSE(queue.BlockingPop(time_to_wait, &frame));
  const base::TimeDelta actual_time_waited =
      base::TimeTicks::HighResNow() - start;

  // Check that we waited at least as long as we asked for.
  EXPECT_GE(actual_time_waited, time_to_wait);
  // Check that we didn't wait too much longer than we asked for.
  EXPECT_LT(actual_time_waited.InMillisecondsF(),
            1.1 * time_to_wait.InMillisecondsF());
}

}  // namespace