// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_LIBRBD_OPERATION_SNAPSHOT_CREATE_REQUEST_H
#define CEPH_LIBRBD_OPERATION_SNAPSHOT_CREATE_REQUEST_H

#include "librbd/operation/Request.h"
#include <iosfwd>
#include <string>

class Context;

namespace librbd {

class ImageCtx;

namespace operation {

class SnapshotCreateRequest : public Request {
public:
  /**
   * Snap Create goes through the following state machine:
   *
   * @verbatim
   *
   *            <start>
   *               |
   *               v
   *           STATE_SUSPEND_REQUESTS
   *               |
   *               v
   *           STATE_SUSPEND_AIO
   *               |
   *               v
   *           STATE_FLUSH_AIO * * * * * * * * * * * * * *
   *               |                                     *
   *   (retry)     v                                     *
   *   . . . > STATE_ALLOCATE_SNAP_ID  * *               *
   *   .           |                     *               *
   *   .           v                     *               *
   *   . . . . STATE_CREATE_SNAP * * * * *               *
   *               |                     *               *
   *               v                     *               *
   *           STATE_CREATE_OBJECT_MAP   *               *
   *               |                     *               *
   *               |                     *               *
   *               |                     v               *
   *               |              STATE_RELEASE_SNAP_ID  *
   *               |                     |               *
   *               |                     v               *
   *               \----------------> <finish> < * * * * *
   *
   * @endverbatim
   *
   * The _CREATE_STATE state may repeat back to the _ALLOCATE_SNAP_ID state
   * if a stale snapshot context is allocated. If the create operation needs
   * to abort, the error path is followed to record the result in the journal
   * (if enabled) and bubble the originating error code back to the client.
   */
  enum State {
    STATE_SUSPEND_REQUESTS,
    STATE_SUSPEND_AIO,
    STATE_FLUSH_AIO,
    STATE_ALLOCATE_SNAP_ID,
    STATE_CREATE_SNAP,
    STATE_CREATE_OBJECT_MAP,
    STATE_RELEASE_SNAP_ID
  };

  SnapshotCreateRequest(ImageCtx &image_ctx, Context *on_finish,
		        const std::string &snap_name);

protected:
  virtual void send_op();
  virtual bool should_complete(int r);

  virtual int filter_return_code(int r) const {
    if (m_ret_val < 0) {
      return m_ret_val;
    }
    return r;
  }

private:
  std::string m_snap_name;
  State m_state;

  int m_ret_val;

  bool m_aio_suspended;
  bool m_requests_suspended;

  uint64_t m_snap_id;
  bool m_snap_created;

  int filter_state_return_code(int r) const {
    if (m_state == STATE_CREATE_SNAP && r == -ESTALE) {
      return 0;
    }
    return r;
  }

  bool should_complete_error();

  void send_suspend_requests();
  void send_suspend_aio();
  void send_flush_aio();
  void send_allocate_snap_id();
  void send_create_snap();
  bool send_create_object_map();
  bool send_release_snap_id();

  void resume_aio();
  void resume_requests();
  void update_snap_context();
};

} // namespace operation
} // namespace librbd

#endif // CEPH_LIBRBD_OPERATION_SNAPSHOT_CREATE_REQUEST_H