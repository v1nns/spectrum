/**
 * \file
 * \brief Structure for question data
 */

#ifndef INCLUDE_MODEL_QUESTION_DATA_H_
#define INCLUDE_MODEL_QUESTION_DATA_H_

#include <functional>
#include <iostream>
#include <string>

namespace model {

/**
 * @brief Content to display on the QuestionDialog
 */
struct QuestionData {
  using Callback = std::function<void()>;

  std::string question;  //!< Message to display on question dialog
  Callback cb_yes;       //!< Callback to trigger when user press "Yes"
  Callback cb_no;        //!< Callback to trigger when user press "No"

  //! Overloaded operators
  friend std::ostream& operator<<(std::ostream& out, const QuestionData& s);
  friend bool operator==(const QuestionData& lhs, const QuestionData& rhs);
  friend bool operator!=(const QuestionData& lhs, const QuestionData& rhs);
};

}  // namespace model

#endif  // INCLUDE_MODEL_QUESTION_DATA_H_
