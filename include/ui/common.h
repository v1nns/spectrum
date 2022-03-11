/**
 * \file
 * \brief  Common structures for Terminal User Interface (TUI)
 */

#ifndef INCLUDE_UI_COMMON_H_
#define INCLUDE_UI_COMMON_H_

#include <assert.h>

#include <tuple>

namespace interface {

/**
 * @brief Coordinates in screen based on a position {x,y}
 */
struct point_t {
  short x, y;
};

/**
 * @brief Screen size based on a maximum value for columns and rows
 */
struct screen_size_t {
  short column, row;

  bool operator!=(const screen_size_t& rhs) const {
    return std::tie(column, row) != std::tie(rhs.column, rhs.row);
  }
};

/**
 * @brief Screen size using a proportion calculated based on the maximum screen size
 */
struct screen_portion_t {
  double column, row;

  /**
   * @brief Construct a new screen portion object
   *
   * @param c Column size (in percentage)
   * @param r Row size (in percentage)
   */
  explicit screen_portion_t(double c, double r) : column(c), row(r) {
    assert(c >= 0 && c <= 1);
    assert(r >= 0 && r <= 1);
  }

  bool operator!=(const screen_portion_t& rhs) const {
    return std::tie(column, row) != std::tie(rhs.column, rhs.row);
  }
};

}  // namespace interface
#endif  // INCLUDE_UI_COMMON_H_