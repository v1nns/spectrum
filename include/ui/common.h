/**
 * \file
 * \brief  Common structures for Terminal User Interface (TUI)
 */

#ifndef INCLUDE_UI_COMMON_H_
#define INCLUDE_UI_COMMON_H_

#include <tuple>

namespace interface {

/**
 * @brief Struct representing a coordinate in screen
 */
struct point_t {
  short x, y;
};

/**
 * @brief Struct representing screen size based on a maximum value for columns and rows
 */
struct screen_size_t {
  short column, row;

  bool operator!=(const screen_size_t& rhs) const {
    return std::tie(column, row) != std::tie(rhs.column, rhs.row);
  }
};

}  // namespace interface
#endif  // INCLUDE_UI_COMMON_H_