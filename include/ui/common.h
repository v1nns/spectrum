/**
 * \file
 * \brief  Common structures for UI
 */

#ifndef INCLUDE_UI_COMMON_H_
#define INCLUDE_UI_COMMON_H_

namespace interface {
struct point_t {
  short x, y;
};

struct screen_size_t {
  short column, row;
};

}  // namespace interface
#endif  // INCLUDE_UI_COMMON_H_